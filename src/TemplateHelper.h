#import <String.h>
#import <HTML/Entities.h>

#import "Body.h"

#define html(...) HTML_Entities_Encode(__VA_ARGS__)

String TemplateHelper_PrintStyled(int style, String s);
