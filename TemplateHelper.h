#import <String.h>
#import <HTML/Entities.h>

#import "Body.h"

typedef union {
	Body_StyleType type;
} OpenStyle;

typedef union {
	Body_StyleType type;
} CloseStyle;

overload void Template_Print(OpenStyle style, String *res);
overload void Template_Print(CloseStyle style, String *res);
