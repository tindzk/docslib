#import "TextBody.h"

#define self TextBody

sdef(void, Process, Body *body, TextDocumentInstance doc) {
	if (body->type == Body_Type_Collection) {
		each(item, body->nodes) {
			scall(Process, *item, doc);
		}
	} else if (body->type == Body_Type_Paragraph) {
		if (!TextDocument_HasTrailingLine(doc)) {
			TextDocument_AddLine(doc);
		}

		each(item, body->nodes) {
			scall(Process, *item, doc);
		}

		TextDocument_AddLine(doc);
	} else if (body->type == Body_Type_List) {
		bool ordered = body->list.ordered;

		TextDocument_Indent(doc);
		TextDocument_AddLine(doc);

		each(item, body->nodes) {
			size_t indent = 2;

			if (ordered) {
				String number = Integer_ToString(getIndex(item, body->nodes));
				indent += number.len;

				TextDocument_Add(doc, number.rd);
				TextDocument_Add(doc, $(". "));

				String_Destroy(&number);
			} else {
				TextDocument_Add(doc, $("- "));
			}

			TextDocument_SetFixedIndent(doc, indent);
			scall(Process, *item, doc);
			TextDocument_SetFixedIndent(doc, 0);

			TextDocument_AddLine(doc);
		}

		TextDocument_Unindent(doc);
	} else if (body->type == Body_Type_ListItem) {
		each(item, body->nodes) {
			scall(Process, *item, doc);
		}
	} else if (body->type == Body_Type_Text) {
		static struct {
			Body_Style style;
			RdString value;
		} styles[] = {
			{ Body_Styles_Bold,     $("**") },
			{ Body_Styles_Italic,   $("//") },
			{ Body_Styles_Term,     $("``") }
		};

		fwd(i, nElems(styles)) {
			if (body->text.style & styles[i].style) {
				TextDocument_Add(doc, styles[i].value);
			}
		}

		TextDocument_Add(doc, body->text.value.rd);

		bwd(i, nElems(styles) - 1) {
			if (body->text.style & styles[i].style) {
				TextDocument_Add(doc, styles[i].value);
			}
		}
	} else if (body->type == Body_Type_Image) {
		TextDocument_Add(doc, $("[image: "));
		TextDocument_Add(doc, body->image.path.rd);
		TextDocument_Add(doc, $("]"));
	} else if (body->type == Body_Type_Jump) {
		each(item, body->nodes) {
			scall(Process, *item, doc);
		}

		TextDocument_Add(doc, $("(cf. '"));
		TextDocument_Add(doc, body->jump.anchor.rd);
		TextDocument_Add(doc, $("')"));
	} else if (body->type == Body_Type_Anchor) {
		TextDocument_Add(doc, $("["));
		TextDocument_Add(doc, body->anchor.name.rd);
		TextDocument_Add(doc, $("]"));
	} else if (body->type == Body_Type_Mail) {
		each(item, body->nodes) {
			scall(Process, *item, doc);
		}

		TextDocument_Add(doc, $(" ("));
		TextDocument_Add(doc, body->mail.addr.rd);
		TextDocument_Add(doc, $(")"));
	} else if (body->type == Body_Type_Url) {
		each(item, body->nodes) {
			scall(Process, *item, doc);
		}

		TextDocument_Add(doc, $(" ("));
		TextDocument_Add(doc, body->url.url.rd);
		TextDocument_Add(doc, $(")"));
	} else if (body->type == Body_Type_Command) {
		TextDocument_Indent(doc);
		TextDocument_AddLine(doc);

		TextDocument_Add(doc, body->command.value.rd);

		TextDocument_Unindent(doc);
		TextDocument_AddLine(doc);
		TextDocument_AddLine(doc);
	} else if (body->type == Body_Type_Code) {
		TextDocument_Indent(doc);
		TextDocument_AddLine(doc);

		TextDocument_Add(doc, body->code.value.rd);

		TextDocument_Unindent(doc);
		TextDocument_AddLine(doc);
	} else if (body->type == Body_Type_Block) {
		TextDocument_AddLine(doc);
		TextDocument_Add(doc,
			(body->block.type == Body_BlockType_Note)    ?  $("[Note]")    :
			(body->block.type == Body_BlockType_Warning) ?  $("[Warning]") :
			(body->block.type == Body_BlockType_Quote)   ?  $("[Quote]")   :
			$(""));

		TextDocument_Indent(doc);
		TextDocument_AddLine(doc);

		each(item, body->nodes) {
			scall(Process, *item, doc);
		}

		TextDocument_Unindent(doc);
		TextDocument_AddLine(doc);
	}
}
