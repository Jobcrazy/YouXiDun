#import "TCPClient.h"

static const NSUInteger TCPMaximumFrameSize = 16 * 1024;
static const NSUInteger TCPMaximumPendingOutputSize = 16 * TCPMaximumFrameSize;
static const NSTimeInterval TCPConnectTimeout = 5.0;

@interface TCPPendingFrame : NSObject
@property (nonatomic, strong) NSData *data;
@property (nonatomic, copy) NSString *message;
@property (nonatomic) NSUInteger offset;
@end

@implementation TCPPendingFrame
@end

@interface TCPClient ()

@property (nonatomic, strong, nullable) NSInputStream *inputStream;
@property (nonatomic, strong, nullable) NSOutputStream *outputStream;
@property (nonatomic, strong) NSMutableData *inputBuffer;
@property (nonatomic, strong) NSMutableArray<TCPPendingFrame *> *pendingFrames;
@property (nonatomic, strong, nullable) NSTimer *connectTimer;
@property (nonatomic, strong, nullable) NSTimer *pingTimer;
@property (nonatomic, copy) NSString *host;
@property (nonatomic) NSInteger port;
@property (nonatomic) uint64_t sequence;
@property (nonatomic) BOOL inputOpened;
@property (nonatomic) BOOL outputOpened;
@property (nonatomic, readwrite, getter=isConnected) BOOL connected;
@property (nonatomic) BOOL terminalEventSent;

@end

@implementation TCPClient

- (instancetype)initWithDelegate:(id<TCPClientDelegate>)delegate {
    self = [super init];
    if (self) {
        _delegate = delegate;
        _inputBuffer = [NSMutableData data];
        _pendingFrames = [NSMutableArray array];
        _host = @"";
    }
    return self;
}

- (void)dealloc {
    [self cleanupStreams];
}

- (void)connectToHost:(NSString *)host port:(NSInteger)port {
    if (![NSThread isMainThread]) {
        dispatch_async(dispatch_get_main_queue(), ^{
            [self connectToHost:host port:port];
        });
        return;
    }

    [self cleanupStreams];
    self.terminalEventSent = NO;
    self.host = host;
    self.port = port;
    self.sequence = 0;
    self.inputOpened = NO;
    self.outputOpened = NO;
    self.connected = NO;
    [self.inputBuffer setLength:0];
    [self.pendingFrames removeAllObjects];

    if (host.length == 0 || port < 1 || port > 65535) {
        [self finishWithMessage:@"Invalid host or port" error:YES];
        return;
    }

    NSInputStream *input = nil;
    NSOutputStream *output = nil;
    [NSStream getStreamsToHostWithName:host
                                  port:port
                           inputStream:&input
                          outputStream:&output];
    if (input == nil || output == nil) {
        [self finishWithMessage:@"Unable to create TCP streams" error:YES];
        return;
    }

    self.inputStream = input;
    self.outputStream = output;
    input.delegate = self;
    output.delegate = self;
    [input scheduleInRunLoop:NSRunLoop.mainRunLoop forMode:NSRunLoopCommonModes];
    [output scheduleInRunLoop:NSRunLoop.mainRunLoop forMode:NSRunLoopCommonModes];
    [input open];
    [output open];

    self.connectTimer = [NSTimer timerWithTimeInterval:TCPConnectTimeout
                                                target:self
                                              selector:@selector(connectTimedOut:)
                                              userInfo:nil
                                               repeats:NO];
    [NSRunLoop.mainRunLoop addTimer:self.connectTimer forMode:NSRunLoopCommonModes];
}

- (void)disconnect {
    if (![NSThread isMainThread]) {
        dispatch_async(dispatch_get_main_queue(), ^{
            [self disconnect];
        });
        return;
    }
    self.terminalEventSent = YES;
    [self cleanupStreams];
}

- (void)connectTimedOut:(NSTimer *)timer {
    if (timer == self.connectTimer && !self.isConnected) {
        [self finishWithMessage:@"Connection timed out" error:YES];
    }
}

- (void)stream:(NSStream *)stream handleEvent:(NSStreamEvent)eventCode {
    switch (eventCode) {
        case NSStreamEventOpenCompleted:
            if (stream == self.inputStream) self.inputOpened = YES;
            if (stream == self.outputStream) self.outputOpened = YES;
            [self completeConnectionIfReady];
            break;

        case NSStreamEventHasBytesAvailable:
            if (stream == self.inputStream) [self readAvailableBytes];
            break;

        case NSStreamEventHasSpaceAvailable:
            if (stream == self.outputStream) [self flushOutputBuffer];
            break;

        case NSStreamEventErrorOccurred: {
            NSString *message = stream.streamError.localizedDescription ?: @"Stream error";
            [self finishWithMessage:message error:!self.isConnected];
            break;
        }

        case NSStreamEventEndEncountered:
            [self finishWithMessage:@"Connection closed" error:NO];
            break;

        default:
            break;
    }
}

- (void)completeConnectionIfReady {
    if (self.isConnected || !self.inputOpened || !self.outputOpened) return;
    self.connected = YES;
    [self.connectTimer invalidate];
    self.connectTimer = nil;
    [self.delegate tcpClientDidConnect:self];
    [self enqueuePing];
    self.pingTimer = [NSTimer timerWithTimeInterval:1.0
                                             target:self
                                           selector:@selector(pingTimerFired:)
                                           userInfo:nil
                                            repeats:YES];
    [NSRunLoop.mainRunLoop addTimer:self.pingTimer forMode:NSRunLoopCommonModes];
}

- (void)pingTimerFired:(NSTimer *)timer {
    if (timer == self.pingTimer && self.isConnected) [self enqueuePing];
}

- (void)enqueuePing {
    NSDateFormatter *formatter = [[NSDateFormatter alloc] init];
    formatter.locale = [NSLocale localeWithLocaleIdentifier:@"en_US_POSIX"];
    formatter.dateFormat = @"HH:mm:ss.SSS";
    NSDictionary *packet = @{
        @"cmd": @"ping",
        @"id": @(self.sequence++),
        @"time": [formatter stringFromDate:[NSDate date]]
    };

    NSError *error = nil;
    NSData *payload = [NSJSONSerialization dataWithJSONObject:packet options:0 error:&error];
    if (payload == nil) {
        [self finishWithMessage:error.localizedDescription ?: @"Unable to encode ping" error:YES];
        return;
    }
    if (payload.length + sizeof(uint32_t) > TCPMaximumFrameSize) {
        [self finishWithMessage:@"Packet is too large" error:YES];
        return;
    }

    uint32_t payloadLength = CFSwapInt32HostToLittle((uint32_t)payload.length);
    NSMutableData *frame = [NSMutableData dataWithBytes:&payloadLength length:sizeof(payloadLength)];
    [frame appendData:payload];
    NSUInteger pendingBytes = 0;
    for (TCPPendingFrame *pending in self.pendingFrames) {
        pendingBytes += pending.data.length - pending.offset;
    }
    if (pendingBytes + frame.length > TCPMaximumPendingOutputSize) {
        [self finishWithMessage:@"Output buffer overflow" error:YES];
        return;
    }

    NSString *text = [[NSString alloc] initWithData:payload encoding:NSUTF8StringEncoding];
    TCPPendingFrame *pending = [[TCPPendingFrame alloc] init];
    pending.data = frame;
    pending.message = text ?: @"";
    [self.pendingFrames addObject:pending];
    [self flushOutputBuffer];
}

- (void)flushOutputBuffer {
    while (self.isConnected && self.pendingFrames.count > 0 && self.outputStream.hasSpaceAvailable) {
        TCPPendingFrame *pending = self.pendingFrames.firstObject;
        const uint8_t *bytes = pending.data.bytes;
        NSUInteger remaining = pending.data.length - pending.offset;
        NSInteger count = [self.outputStream write:bytes + pending.offset maxLength:remaining];
        if (count < 0) {
            NSString *message = self.outputStream.streamError.localizedDescription ?: @"Send failed";
            [self finishWithMessage:[@"Send failed: " stringByAppendingString:message] error:YES];
            return;
        }
        if (count == 0) return;
        pending.offset += (NSUInteger)count;
        if (pending.offset == pending.data.length) {
            [self.pendingFrames removeObjectAtIndex:0];
            [self.delegate tcpClient:self didSendMessage:pending.message];
        }
    }
}

- (void)readAvailableBytes {
    uint8_t bytes[4096];
    while (self.inputStream.hasBytesAvailable) {
        NSInteger count = [self.inputStream read:bytes maxLength:sizeof(bytes)];
        if (count < 0) {
            NSString *message = self.inputStream.streamError.localizedDescription ?: @"Receive failed";
            [self finishWithMessage:message error:NO];
            return;
        }
        if (count == 0) {
            [self finishWithMessage:@"Connection closed" error:NO];
            return;
        }
        [self.inputBuffer appendBytes:bytes length:(NSUInteger)count];
        if (![self processInputFrames]) return;
    }
}

- (BOOL)processInputFrames {
    while (self.inputBuffer.length >= sizeof(uint32_t)) {
        uint32_t littleEndianLength = 0;
        [self.inputBuffer getBytes:&littleEndianLength length:sizeof(littleEndianLength)];
        NSUInteger payloadLength = CFSwapInt32LittleToHost(littleEndianLength);
        if (payloadLength == 0 || payloadLength + sizeof(uint32_t) > TCPMaximumFrameSize) {
            [self finishWithMessage:[NSString stringWithFormat:@"Invalid packet length: %lu",
                                     (unsigned long)payloadLength]
                              error:YES];
            return NO;
        }

        NSUInteger frameLength = sizeof(uint32_t) + payloadLength;
        if (self.inputBuffer.length < frameLength) return YES;
        NSData *payload = [self.inputBuffer subdataWithRange:NSMakeRange(sizeof(uint32_t), payloadLength)];
        [self.inputBuffer replaceBytesInRange:NSMakeRange(0, frameLength) withBytes:NULL length:0];
        NSString *message = [[NSString alloc] initWithData:payload encoding:NSUTF8StringEncoding];
        if (message == nil) {
            [self finishWithMessage:@"Received payload is not valid UTF-8" error:YES];
            return NO;
        }
        [self.delegate tcpClient:self didReceiveMessage:message];
    }
    return YES;
}

- (void)finishWithMessage:(NSString *)message error:(BOOL)error {
    if (self.terminalEventSent) return;
    self.terminalEventSent = YES;
    BOOL wasConnected = self.isConnected;
    [self cleanupStreams];
    if (error || !wasConnected) {
        [self.delegate tcpClient:self didFailWithError:message];
    } else {
        [self.delegate tcpClient:self didDisconnectWithReason:message];
    }
}

- (void)cleanupStreams {
    [self.connectTimer invalidate];
    [self.pingTimer invalidate];
    self.connectTimer = nil;
    self.pingTimer = nil;

    self.inputStream.delegate = nil;
    self.outputStream.delegate = nil;
    [self.inputStream close];
    [self.outputStream close];
    [self.inputStream removeFromRunLoop:NSRunLoop.mainRunLoop forMode:NSRunLoopCommonModes];
    [self.outputStream removeFromRunLoop:NSRunLoop.mainRunLoop forMode:NSRunLoopCommonModes];
    self.inputStream = nil;
    self.outputStream = nil;
    self.inputOpened = NO;
    self.outputOpened = NO;
    self.connected = NO;
    [self.inputBuffer setLength:0];
    [self.pendingFrames removeAllObjects];
}

@end
