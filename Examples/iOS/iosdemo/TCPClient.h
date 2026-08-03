#import <Foundation/Foundation.h>

NS_ASSUME_NONNULL_BEGIN

@class TCPClient;

@protocol TCPClientDelegate <NSObject>

- (void)tcpClientDidConnect:(TCPClient *)client;
- (void)tcpClient:(TCPClient *)client didDisconnectWithReason:(NSString *)reason;
- (void)tcpClient:(TCPClient *)client didFailWithError:(NSString *)message;
- (void)tcpClient:(TCPClient *)client didSendMessage:(NSString *)message;
- (void)tcpClient:(TCPClient *)client didReceiveMessage:(NSString *)message;

@end

@interface TCPClient : NSObject <NSStreamDelegate>

@property (nonatomic, weak, nullable) id<TCPClientDelegate> delegate;
@property (nonatomic, readonly, getter=isConnected) BOOL connected;

- (instancetype)initWithDelegate:(id<TCPClientDelegate>)delegate;
- (void)connectToHost:(NSString *)host port:(NSInteger)port;
- (void)disconnect;

@end

NS_ASSUME_NONNULL_END
