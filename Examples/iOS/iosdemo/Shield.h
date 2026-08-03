//
//  shield.h
//  shield
//
//  Created by Liu Hang on 2023-05-30.
//

#import <Foundation/Foundation.h>

typedef NS_ENUM(NSInteger, InitError) {
    ERROR_INIT_SUCCESS = 0,
    ERROR_INIT_APPID,
    ERROR_INIT_MID,
    ERROR_INIT_CONFIG,
    ERROR_INIT_PORT,
    ERROR_INIT_CONFDIR,
    ERROR_INIT_USER,
    ERROR_INIT_SO,
    ERROR_INIT_FUNC,
    ERROR_INIT_HOST,
};

@interface Shield : NSObject

+ (instancetype)getInstance;
- (NSInteger)Init:(NSString *)host key:(NSString *)key;

@end
