#import "TemplateHelper.h"

static struct {
	Body_Style style;
	String     tag;
	String     options;
} styles[] = {
	{ Body_Styles_Bold,     $("b"),    $("")                   },
	{ Body_Styles_Italic,   $("i"),    $("")                   },
	{ Body_Styles_Class,    $("span"), $("class=\"class\"")    },
	{ Body_Styles_Function, $("span"), $("class=\"function\"") },
	{ Body_Styles_Variable, $("span"), $("class=\"variable\"") },
	{ Body_Styles_Macro,    $("span"), $("class=\"macro\"")    },
	{ Body_Styles_Term,     $("span"), $("class=\"term\"")     },
	{ Body_Styles_Keyword,  $("span"), $("class=\"keyword\"")  },
	{ Body_Styles_Path,     $("span"), $("class=\"path\"")     },
	{ Body_Styles_Number,   $("span"), $("class=\"number\"")   }
};

overload void Template_Print(int style, String s, String *res) {
	forward(i, nElems(styles)) {
		if (style & styles[i].style) {
			String_Append(res, $("<"));
			String_Append(res, styles[i].tag);

			if (styles[i].options.len > 0) {
				String_Append(res, $(" "));
				String_Append(res, styles[i].options);
			}

			String_Append(res, $(">"));
		}
	}

	HTML_Entities_Encode(s, res);

	reverse(i, nElems(styles)) {
		if (style & styles[i].style) {
			String_Append(res, $("</"));
			String_Append(res, styles[i].tag);
			String_Append(res, $(">"));
		}
	}
}