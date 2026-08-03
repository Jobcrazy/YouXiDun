#import "ViewController.h"
#import "Shield.h"
#import "TCPClient.h"

typedef NS_ENUM(NSInteger, DemoLogType) {
    DemoLogTypeInfo,
    DemoLogTypePing,
    DemoLogTypePong,
    DemoLogTypeError,
    DemoLogTypeWarning
};

@interface DemoLogEntry : NSObject
@property (nonatomic, copy) NSString *time;
@property (nonatomic, copy) NSString *message;
@property (nonatomic) DemoLogType type;
@end

@implementation DemoLogEntry
@end

@interface DemoLogCell : UITableViewCell
@property (nonatomic, strong) UIView *indicatorView;
@property (nonatomic, strong) UILabel *timeLabel;
@property (nonatomic, strong) UILabel *messageLabel;
@end

@implementation DemoLogCell

- (instancetype)initWithStyle:(UITableViewCellStyle)style reuseIdentifier:(NSString *)reuseIdentifier {
    self = [super initWithStyle:style reuseIdentifier:reuseIdentifier];
    if (self) {
        self.backgroundColor = [UIColor colorWithRed:13.0 / 255.0 green:17.0 / 255.0 blue:23.0 / 255.0 alpha:1.0];
        self.selectionStyle = UITableViewCellSelectionStyleNone;

        _indicatorView = [[UIView alloc] initWithFrame:CGRectZero];
        _timeLabel = [[UILabel alloc] initWithFrame:CGRectZero];
        _messageLabel = [[UILabel alloc] initWithFrame:CGRectZero];
        _timeLabel.font = [UIFont fontWithName:@"Menlo" size:10.0] ?: [UIFont systemFontOfSize:10.0];
        _messageLabel.font = [UIFont fontWithName:@"Menlo" size:12.0] ?: [UIFont systemFontOfSize:12.0];
        _messageLabel.lineBreakMode = NSLineBreakByTruncatingTail;
        [self.contentView addSubview:_indicatorView];
        [self.contentView addSubview:_timeLabel];
        [self.contentView addSubview:_messageLabel];
    }
    return self;
}

- (void)layoutSubviews {
    [super layoutSubviews];
    CGFloat height = CGRectGetHeight(self.contentView.bounds);
    self.indicatorView.frame = CGRectMake(2.0, 4.0, 3.0, MAX(16.0, height - 8.0));
    self.timeLabel.frame = CGRectMake(13.0, 0.0, 88.0, height);
    self.messageLabel.frame = CGRectMake(109.0, 0.0, MAX(0.0, CGRectGetWidth(self.contentView.bounds) - 111.0), height);
}

@end

@interface ViewController () <TCPClientDelegate, UITableViewDataSource, UITableViewDelegate, UITextFieldDelegate>

@property (nonatomic, strong) UILabel *statusLabel;
@property (nonatomic, strong) UITextField *hostField;
@property (nonatomic, strong) UITextField *portField;
@property (nonatomic, strong) UIButton *connectButton;
@property (nonatomic, strong) UIButton *disconnectButton;
@property (nonatomic, strong) UITableView *logTableView;
@property (nonatomic, strong) NSMutableArray<DemoLogEntry *> *logs;
@property (nonatomic, strong, nullable) TCPClient *client;
@property (nonatomic, strong) NSDateFormatter *logTimeFormatter;

@end

static const NSUInteger DemoMaximumLogCount = 200;

@implementation ViewController

- (void)viewDidLoad {
    [super viewDidLoad];
    self.logs = [NSMutableArray array];
    self.logTimeFormatter = [[NSDateFormatter alloc] init];
    self.logTimeFormatter.locale = [NSLocale localeWithLocaleIdentifier:@"en_US_POSIX"];
    self.logTimeFormatter.dateFormat = @"HH:mm:ss.SSS";
    [self buildInterface];
    [self applyDisconnectedState:@"Not connected"];

    NSInteger result = [[Shield getInstance] Init:nil key:@"ac7f95bb-1e4d-4186-8e01-e6334462a608"];
    if (result == ERROR_INIT_SUCCESS) {
        [self appendLog:@"SDK initialized successfully" type:DemoLogTypeInfo];
    } else {
        [self appendLog:[NSString stringWithFormat:@"SDK initialization failed: %ld", (long)result]
                    type:DemoLogTypeError];
    }
}

- (void)dealloc {
    self.client.delegate = nil;
    [self.client disconnect];
}

- (void)buildInterface {
    self.view.backgroundColor = [self colorWithHex:0x0D1117];

    UILabel *titleLabel = [[UILabel alloc] init];
    titleLabel.translatesAutoresizingMaskIntoConstraints = NO;
    titleLabel.text = @"NetGuard SDK Demo";
    titleLabel.textColor = [self colorWithHex:0xE0E6ED];
    titleLabel.font = [UIFont fontWithName:@"Menlo-Bold" size:20.0] ?: [UIFont boldSystemFontOfSize:20.0];

    self.statusLabel = [[UILabel alloc] init];
    self.statusLabel.translatesAutoresizingMaskIntoConstraints = NO;
    self.statusLabel.font = [UIFont fontWithName:@"Menlo" size:13.0] ?: [UIFont systemFontOfSize:13.0];
    self.statusLabel.textAlignment = NSTextAlignmentRight;

    self.hostField = [self endpointFieldWithPlaceholder:@"Host" value:@"127.0.0.1"];
    self.hostField.keyboardType = UIKeyboardTypeURL;
    self.portField = [self endpointFieldWithPlaceholder:@"Port" value:@"10000"];
    self.portField.keyboardType = UIKeyboardTypeNumberPad;

    self.connectButton = [self actionButtonWithTitle:@"Connect" color:[self colorWithHex:0x4FC3F7]];
    [self.connectButton addTarget:self action:@selector(connectTapped) forControlEvents:UIControlEventTouchUpInside];
    self.disconnectButton = [self actionButtonWithTitle:@"Disconnect" color:[self colorWithHex:0xEF5350]];
    [self.disconnectButton addTarget:self action:@selector(disconnectTapped) forControlEvents:UIControlEventTouchUpInside];

    UIView *divider = [[UIView alloc] init];
    divider.translatesAutoresizingMaskIntoConstraints = NO;
    divider.backgroundColor = [self colorWithHex:0x21262D];

    self.logTableView = [[UITableView alloc] initWithFrame:CGRectZero style:UITableViewStylePlain];
    self.logTableView.translatesAutoresizingMaskIntoConstraints = NO;
    self.logTableView.backgroundColor = self.view.backgroundColor;
    self.logTableView.separatorStyle = UITableViewCellSeparatorStyleNone;
    self.logTableView.rowHeight = 28.0;
    self.logTableView.dataSource = self;
    self.logTableView.delegate = self;
    self.logTableView.keyboardDismissMode = UIScrollViewKeyboardDismissModeOnDrag;
    [self.logTableView registerClass:DemoLogCell.class forCellReuseIdentifier:@"LogCell"];

    UIStackView *header = [[UIStackView alloc] initWithArrangedSubviews:@[titleLabel, self.statusLabel]];
    header.translatesAutoresizingMaskIntoConstraints = NO;
    header.axis = UILayoutConstraintAxisHorizontal;
    header.alignment = UIStackViewAlignmentCenter;
    header.spacing = 8.0;

    UIStackView *endpointRow = [[UIStackView alloc] initWithArrangedSubviews:@[self.hostField, self.portField]];
    endpointRow.translatesAutoresizingMaskIntoConstraints = NO;
    endpointRow.axis = UILayoutConstraintAxisHorizontal;
    endpointRow.spacing = 6.0;
    endpointRow.distribution = UIStackViewDistributionFill;

    UIStackView *buttonRow = [[UIStackView alloc] initWithArrangedSubviews:@[self.connectButton, self.disconnectButton]];
    buttonRow.translatesAutoresizingMaskIntoConstraints = NO;
    buttonRow.axis = UILayoutConstraintAxisHorizontal;
    buttonRow.spacing = 8.0;
    buttonRow.distribution = UIStackViewDistributionFillEqually;

    [self.view addSubview:header];
    [self.view addSubview:endpointRow];
    [self.view addSubview:buttonRow];
    [self.view addSubview:divider];
    [self.view addSubview:self.logTableView];

    UILayoutGuide *topGuide = self.topLayoutGuide;
    [NSLayoutConstraint activateConstraints:@[
        [header.topAnchor constraintEqualToAnchor:topGuide.bottomAnchor constant:12.0],
        [header.leadingAnchor constraintEqualToAnchor:self.view.leadingAnchor constant:12.0],
        [header.trailingAnchor constraintEqualToAnchor:self.view.trailingAnchor constant:-12.0],
        [self.statusLabel.widthAnchor constraintGreaterThanOrEqualToConstant:100.0],

        [endpointRow.topAnchor constraintEqualToAnchor:header.bottomAnchor constant:12.0],
        [endpointRow.leadingAnchor constraintEqualToAnchor:header.leadingAnchor],
        [endpointRow.trailingAnchor constraintEqualToAnchor:header.trailingAnchor],
        [endpointRow.heightAnchor constraintEqualToConstant:40.0],
        [self.hostField.widthAnchor constraintEqualToAnchor:self.portField.widthAnchor multiplier:3.0],

        [buttonRow.topAnchor constraintEqualToAnchor:endpointRow.bottomAnchor constant:8.0],
        [buttonRow.leadingAnchor constraintEqualToAnchor:header.leadingAnchor],
        [buttonRow.trailingAnchor constraintEqualToAnchor:header.trailingAnchor],
        [buttonRow.heightAnchor constraintEqualToConstant:40.0],

        [divider.topAnchor constraintEqualToAnchor:buttonRow.bottomAnchor constant:10.0],
        [divider.leadingAnchor constraintEqualToAnchor:header.leadingAnchor],
        [divider.trailingAnchor constraintEqualToAnchor:header.trailingAnchor],
        [divider.heightAnchor constraintEqualToConstant:1.0],

        [self.logTableView.topAnchor constraintEqualToAnchor:divider.bottomAnchor constant:8.0],
        [self.logTableView.leadingAnchor constraintEqualToAnchor:header.leadingAnchor],
        [self.logTableView.trailingAnchor constraintEqualToAnchor:header.trailingAnchor],
        [self.logTableView.bottomAnchor constraintEqualToAnchor:self.bottomLayoutGuide.topAnchor]
    ]];
}

- (UITextField *)endpointFieldWithPlaceholder:(NSString *)placeholder value:(NSString *)value {
    UITextField *field = [[UITextField alloc] init];
    field.translatesAutoresizingMaskIntoConstraints = NO;
    field.placeholder = placeholder;
    field.text = value;
    field.textColor = [self colorWithHex:0xC9D1D9];
    field.font = [UIFont fontWithName:@"Menlo" size:13.0] ?: [UIFont systemFontOfSize:13.0];
    field.backgroundColor = [self colorWithHex:0x161B22];
    field.layer.borderColor = [self colorWithHex:0x30363D].CGColor;
    field.layer.borderWidth = 1.0;
    field.layer.cornerRadius = 4.0;
    field.leftView = [[UIView alloc] initWithFrame:CGRectMake(0, 0, 10, 1)];
    field.leftViewMode = UITextFieldViewModeAlways;
    field.clearButtonMode = UITextFieldViewModeWhileEditing;
    field.autocorrectionType = UITextAutocorrectionTypeNo;
    field.autocapitalizationType = UITextAutocapitalizationTypeNone;
    field.delegate = self;
    field.attributedPlaceholder = [[NSAttributedString alloc] initWithString:placeholder
                                                                  attributes:@{NSForegroundColorAttributeName: [self colorWithHex:0x4A5568]}];
    return field;
}

- (UIButton *)actionButtonWithTitle:(NSString *)title color:(UIColor *)color {
    UIButton *button = [UIButton buttonWithType:UIButtonTypeSystem];
    button.translatesAutoresizingMaskIntoConstraints = NO;
    button.backgroundColor = color;
    button.layer.cornerRadius = 4.0;
    button.titleLabel.font = [UIFont fontWithName:@"Menlo" size:14.0] ?: [UIFont systemFontOfSize:14.0];
    [button setTitle:title forState:UIControlStateNormal];
    [button setTitleColor:[self colorWithHex:0x0D1117] forState:UIControlStateNormal];
    return button;
}

- (void)connectTapped {
    [self.view endEditing:YES];
    NSString *host = [self.hostField.text stringByTrimmingCharactersInSet:NSCharacterSet.whitespaceAndNewlineCharacterSet];
    NSString *portText = [self.portField.text stringByTrimmingCharactersInSet:NSCharacterSet.whitespaceAndNewlineCharacterSet];
    if (host.length == 0 || portText.length == 0) {
        [self appendLog:@"⚠ Enter a host and port" type:DemoLogTypeWarning];
        return;
    }

    NSScanner *scanner = [NSScanner scannerWithString:portText];
    NSInteger port = 0;
    if (![scanner scanInteger:&port] || !scanner.isAtEnd || port < 1 || port > 65535) {
        [self appendLog:@"⚠ Port must be between 1 and 65535" type:DemoLogTypeWarning];
        return;
    }

    [self.client disconnect];
    self.client = [[TCPClient alloc] initWithDelegate:self];
    [self applyConnectingState];
    [self appendLog:[NSString stringWithFormat:@"→ Connecting to %@:%ld", host, (long)port]
                type:DemoLogTypeInfo];
    [self.client connectToHost:host port:port];
}

- (void)disconnectTapped {
    [self.client disconnect];
    self.client = nil;
    [self applyDisconnectedState:@"Not connected"];
    [self appendLog:@"• Disconnected manually" type:DemoLogTypeInfo];
}

- (void)applyConnectingState {
    self.connectButton.hidden = YES;
    self.disconnectButton.hidden = NO;
    self.statusLabel.text = @"Connecting...";
    self.statusLabel.textColor = [self colorWithHex:0xEF5350];
}

- (void)applyConnectedState {
    self.connectButton.hidden = YES;
    self.disconnectButton.hidden = NO;
    self.statusLabel.text = @"Connected";
    self.statusLabel.textColor = [self colorWithHex:0x81C784];
}

- (void)applyDisconnectedState:(NSString *)status {
    self.connectButton.hidden = NO;
    self.disconnectButton.hidden = YES;
    self.statusLabel.text = status;
    self.statusLabel.textColor = [self colorWithHex:0xEF5350];
}

- (void)appendLog:(NSString *)message type:(DemoLogType)type {
    DemoLogEntry *entry = [[DemoLogEntry alloc] init];
    entry.time = [self.logTimeFormatter stringFromDate:[NSDate date]];
    entry.message = message;
    entry.type = type;
    if (self.logs.count >= DemoMaximumLogCount) [self.logs removeObjectAtIndex:0];
    [self.logs addObject:entry];
    [self.logTableView reloadData];
    NSIndexPath *last = [NSIndexPath indexPathForRow:(NSInteger)self.logs.count - 1 inSection:0];
    [self.logTableView scrollToRowAtIndexPath:last atScrollPosition:UITableViewScrollPositionBottom animated:NO];
}

- (NSInteger)tableView:(UITableView *)tableView numberOfRowsInSection:(NSInteger)section {
    return (NSInteger)self.logs.count;
}

- (UITableViewCell *)tableView:(UITableView *)tableView cellForRowAtIndexPath:(NSIndexPath *)indexPath {
    DemoLogCell *cell = [tableView dequeueReusableCellWithIdentifier:@"LogCell" forIndexPath:indexPath];
    DemoLogEntry *entry = self.logs[(NSUInteger)indexPath.row];
    UIColor *color = [self colorForLogType:entry.type];
    cell.indicatorView.backgroundColor = color;
    cell.timeLabel.text = entry.time;
    cell.timeLabel.textColor = [self colorWithHex:0x78909C];
    cell.messageLabel.text = entry.message;
    cell.messageLabel.textColor = color;
    return cell;
}

- (BOOL)textFieldShouldReturn:(UITextField *)textField {
    [textField resignFirstResponder];
    return YES;
}

- (void)tcpClientDidConnect:(TCPClient *)client {
    if (client != self.client) return;
    [self applyConnectedState];
    [self appendLog:@"✓ Connected successfully" type:DemoLogTypeInfo];
}

- (void)tcpClient:(TCPClient *)client didDisconnectWithReason:(NSString *)reason {
    if (client != self.client) return;
    self.client = nil;
    [self applyDisconnectedState:@"Disconnected"];
    [self appendLog:[@"✗ Disconnected: " stringByAppendingString:reason] type:DemoLogTypeError];
}

- (void)tcpClient:(TCPClient *)client didFailWithError:(NSString *)message {
    if (client != self.client) return;
    self.client = nil;
    [self applyDisconnectedState:@"Connection failed"];
    [self appendLog:[@"✗ " stringByAppendingString:message] type:DemoLogTypeError];
}

- (void)tcpClient:(TCPClient *)client didSendMessage:(NSString *)message {
    if (client == self.client) [self appendLog:[@"↑ " stringByAppendingString:message] type:DemoLogTypePing];
}

- (void)tcpClient:(TCPClient *)client didReceiveMessage:(NSString *)message {
    if (client == self.client) [self appendLog:[@"↓ " stringByAppendingString:message] type:DemoLogTypePong];
}

- (UIColor *)colorForLogType:(DemoLogType)type {
    switch (type) {
        case DemoLogTypePing: return [self colorWithHex:0x4FC3F7];
        case DemoLogTypePong: return [self colorWithHex:0x81C784];
        case DemoLogTypeError: return [self colorWithHex:0xEF5350];
        case DemoLogTypeWarning: return [self colorWithHex:0xFFB74D];
        default: return [self colorWithHex:0xB0BEC5];
    }
}

- (UIColor *)colorWithHex:(NSUInteger)hex {
    return [UIColor colorWithRed:((hex >> 16) & 0xFF) / 255.0
                           green:((hex >> 8) & 0xFF) / 255.0
                            blue:(hex & 0xFF) / 255.0
                           alpha:1.0];
}

@end
