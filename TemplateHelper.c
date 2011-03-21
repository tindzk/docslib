#import "TemplateHelper.h"

static struct {
	RdString tag;
	RdString options;
} styles[] = {
	[Body_Styles_Bold]     = { $("b"),    $("")                   },
	[Body_Styles_Italic]   = { $("i"),    $("")                   },
	[Body_Styles_Class]    = { $("span"), $("class=\"class\"")    },
	[Body_Styles_Function] = { $("span"), $("class=\"function\"") },
	[Body_Styles_Variable] = { $("span"), $("class=\"variable\"") },
	[Body_Styles_Macro]    = { $("span"), $("class=\"macro\"")    },
	[Body_Styles_Term]     = { $("span"), $("class=\"term\"")     },
	[Body_Styles_Keyword]  = { $("span"), $("class=\"keyword\"")  },
	[Body_Styles_Path]     = { $("span"), $("class=\"path\"")     },
	[Body_Styles_Number]   = { $("span"), $("class=\"number\"")   }
};

overload void Template_Print(OpenStyle style, String *res) {
	Body_StyleType type = style.type;

	if (type >= nElems(styles)) {
		return;
	}

	String_Append(res, $("<"));
	String_Append(res, styles[type].tag);

	if (styles[type].options.len > 0) {
		String_Append(res, $(" "));
		String_Append(res, styles[type].options);
	}

	String_Append(res, $(">"));
}

overload void Template_Print(CloseStyle style, String *res) {
	Body_StyleType type = style.type;

	if (type >= nElems(styles)) {
		return;
	}

	String_Append(res, $("</"));
	String_Append(res, styles[type].tag);
	String_Append(res, $(">"));
}
