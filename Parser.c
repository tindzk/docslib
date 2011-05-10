#import "Parser.h"

#define self Parser

static struct {
	RdString       name;
	Body_StyleType style;
} styles[] = {
	{ $("b"),       Body_Styles_Bold     },
	{ $("i"),       Body_Styles_Italic   },
	{ $("class"),   Body_Styles_Class    },
	{ $("func"),    Body_Styles_Function },
	{ $("var"),     Body_Styles_Variable },
	{ $("macro"),   Body_Styles_Macro    },
	{ $("term"),    Body_Styles_Term     },
	{ $("keyword"), Body_Styles_Keyword  },
	{ $("path"),    Body_Styles_Path     },
	{ $("number"),  Body_Styles_Number   }
};

static struct {
	RdString       name;
	Body_BlockType block;
} blocks[] = {
	{ $("note"),    Body_BlockType_Note    },
	{ $("warning"), Body_BlockType_Warning },
	{ $("quote"),   Body_BlockType_Quote   }
};

static sdef(Body_StyleType, ResolveStyle, RdString name) {
	fwd(i, nElems(styles)) {
		if (String_Equals(name, styles[i].name)) {
			return styles[i].style;
		}
	}

	return Body_Styles_None;
}

static sdef(Body_BlockType, ResolveBlock, RdString name) {
	fwd(i, nElems(blocks)) {
		if (String_Equals(name, blocks[i].name)) {
			return blocks[i].block;
		}
	}

	return Body_BlockType_None;
}

rsdef(self, New, Logger *logger) {
	return (self) {
		.tyo       = Typography_New(),
		.logger    = logger,
		.footnotes = BodyArray_New(1024)
	};
}

def(void, Destroy) {
	Typography_Destroy(&this->tyo);

	BodyArray_Destroy(this->footnotes);
	BodyArray_Free(this->footnotes);
}

def(void, SetAutoDetectParagraphs, bool value) {
	this->autoDetectParagraphs = value;
}

def(BodyArray *, GetFootnotes) {
	return this->footnotes;
}

def(Typography_Node *, GetRoot) {
	return Typography_GetRoot(&this->tyo);
}

def(void, Parse, RdString path) {
	File file = File_New(path, FileStatus_ReadOnly);

	BufferedStream stream = BufferedStream_New(File_AsStream(&file));
	BufferedStream_SetInputBuffer(&stream, 1024, 128);

	Typography_Parse(&this->tyo, BufferedStream_AsStream(&stream));

	BufferedStream_Close(&stream);
	BufferedStream_Destroy(&stream);
}

static def(RdString, GetValue, Typography_Node *node) {
	if (node->len == 0) {
		String line = Integer_ToString(node->line);
		Logger_Error(this->logger, $("line %: value expected."), line.rd);
		String_Destroy(&line);

		return $("");
	}

	if (node->len > 1) {
		String line = Integer_ToString(node->line);
		Logger_Error(this->logger, $("line %: too many nodes."), line.rd);
		String_Destroy(&line);

		return $("");
	}

	Typography_Node *child = node->buf[0];

	if (child->type != Typography_NodeType_Text) {
		String line = Integer_ToString(child->line);
		Logger_Error(this->logger,
			$("line %: node given, text expected."),
			line.rd);
		String_Destroy(&line);

		return $("");
	}

	return Typography_Text_GetValue(child);
}

static def(Body *, Enter, BodyArray **arr) {
	Body *body = Body_New(Body_Type_Collection);
	BodyArray_Push(arr, body);

	return body;
}

static def(void, ParseText, Body *body, Typography_Node *node, ref(Handler) *handlers);

static def(void, ParseList, Body *body, Typography_Node *node) {
	RdString options = String_Trim(Typography_Item_GetOptions(node));
	bool ordered = String_Equals(options, $("ordered"));

	Body *list = call(Enter, &body->nodes);
	list->type = Body_Type_List;
	list->list.ordered = ordered;

	fwd(i, node->len) {
		Typography_Node *child = node->buf[i];

		if (child->type == Typography_NodeType_Item) {
			if (!String_Equals(Typography_Item_GetName(child), $("item"))) {
				String line = Integer_ToString(child->line);
				Logger_Error(this->logger,
					$("line %: got '%', 'item' expected."),
					line.rd, Typography_Item_GetName(child));
				String_Destroy(&line);

				continue;
			}

			Body *listItem = call(Enter, &list->nodes);
			listItem->type = Body_Type_ListItem;

			call(ParseText, listItem, child, NULL);
		}
	}
}

static def(String, CleanValue, RdString value) {
	ssize_t pos = String_Find(value, '\n');

	if (pos != String_NotFound) {
		RdString firstLine = String_Slice(value, 0, pos);

		/* Skip first line if empty. */
		if (String_Trim(firstLine).len == 0) {
			value = String_Slice(value, pos + 1);
		}
	}

	value = String_Trim(value, String_TrimRight);

	RdStringArray *lines = String_Split(value, '\n');

	CarrierStringArray *dest = CarrierStringArray_New(lines->len);

	ssize_t unindent = -1;

	each(line, lines) {
		CarrierStringArray_Push(&dest, String_ToCarrier(RdString_Exalt(*line)));

		if (line->len == 0) {
			continue;
		}

		size_t tabs = 0;

		fwd(i, line->len) {
			if (line->buf[i] == '\t') {
				tabs++;
			} else {
				break;
			}
		}

		if (unindent == -1 || unindent > (ssize_t) tabs) {
			unindent = tabs;
		}
	}

	RdStringArray_Free(lines);

	each(line, dest) {
		if (line->len > 0) {
			line->rd = String_Slice(line->rd, unindent);

			/* Replace all leading tabs by 4 spaces as there is no
			 * `tab-stops' property in CSS.
			 * See also http://www.pixelastic.com/blog/79:setting-the-size-of-a-tab-character-in-a-element
			 */

			size_t tabs = 0;

			fwd(i, line->len) {
				if (line->buf[i] == '\t') {
					tabs++;
				} else {
					break;
				}
			}

			if (tabs > 0) {
				String new = String_New(line->len - tabs + tabs * 4);

				rpt(tabs * 4) {
					new.buf[new.len] = ' ';
					new.len++;
				}

				String_Append(&new, String_Slice(line->rd, tabs));

				CarrierString_Assign(line, String_ToCarrier(new));
			}
		}
	}

	RdStringArray *copy = RdStringArray_New(dest->len);

	fwd(i, dest->len) {
		copy->buf[i] = dest->buf[i].rd;
	}

	copy->len = dest->len;

	String res = RdStringArray_Join(copy, $("\n"));

	RdStringArray_Free(copy);

	/* Free all lines in which the tabs were replaced. */
	CarrierStringArray_Destroy(dest);
	CarrierStringArray_Free(dest);

	return res;
}

static def(void, ParseCommand, Body *body, Typography_Node *child) {
	RdString value = call(GetValue,   child);
	String cleaned = call(CleanValue, value);

	Body *elem = call(Enter, &body->nodes);
	elem->type          = Body_Type_Command;
	elem->command.value = cleaned;
}

static def(void, ParseCode, Body *body, Typography_Node *child) {
	RdString value = call(GetValue,   child);
	String cleaned = call(CleanValue, value);

	Body *elem = call(Enter, &body->nodes);
	elem->type       = Body_Type_Code;
	elem->code.value = cleaned;
}

static def(void, ParseMath, Body *body, Typography_Node *child) {
	RdString value = call(GetValue, child);

	Body *elem = call(Enter, &body->nodes);
	elem->type       = Body_Type_Math;
	elem->math.value = String_Clone(value);
}

static def(void, ParseMail, Body *body, Typography_Node *child) {
	RdString addr = String_Trim(Typography_Item_GetOptions(child));

	Body *elem = call(Enter, &body->nodes);
	elem->type      = Body_Type_Mail;
	elem->mail.addr = String_Clone(addr);

	call(ParseText, elem, child, NULL);
}

static def(void, ParseAnchor, Body *body, Typography_Node *child) {
	RdString name = String_Trim(call(GetValue, child));

	Body *elem = call(Enter, &body->nodes);
	elem->type        = Body_Type_Anchor;
	elem->anchor.name = String_Clone(name);
}

static def(void, ParseJump, Body *body, Typography_Node *child) {
	RdString anchor = String_Trim(Typography_Item_GetOptions(child));

	Body *elem = call(Enter, &body->nodes);
	elem->type        = Body_Type_Jump;
	elem->jump.anchor = String_Clone(anchor);

	call(ParseText, elem, child, NULL);
}

static def(void, ParseUrl, Body *body, Typography_Node *child) {
	RdString url = String_Trim(Typography_Item_GetOptions(child));

	Body *elem = call(Enter, &body->nodes);
	elem->type    = Body_Type_Url;
	elem->url.url = String_Clone(url);

	call(ParseText, elem, child, NULL);
}

static def(void, ParseImage, Body *body, Typography_Node *child) {
	RdString path = String_Trim(call(GetValue, child));

	Body *elem = call(Enter, &body->nodes);
	elem->type       = Body_Type_Image;
	elem->image.path = String_Clone(path);
}

static def(void, ParseParagraph, Body *body, Typography_Node *child) {
	Body *elem = call(Enter, &body->nodes);
	elem->type = Body_Type_Paragraph;

	call(ParseText, elem, child, NULL);
}

static def(void, ParseBlock, Body *body, Body_BlockType type, Typography_Node *child) {
	Body *elem = call(Enter, &body->nodes);
	elem->type       = Body_Type_Block;
	elem->block.type = type;

	call(ParseText, elem, child, NULL);
}

static def(void, ParseStyle, Body *body, Body_StyleType type, Typography_Node *child) {
	Body *elem = call(Enter, &body->nodes);
	elem->type        = Body_Type_Style;
	elem->style.type = type;

	call(ParseText, elem, child, NULL);
}

static def(void, ParseFootnote, Body *body, Typography_Node *child) {
	Body *elem = call(Enter, &this->footnotes);
	call(ParseText, elem, child, NULL);

	Body *elem2 = call(Enter, &body->nodes);
	elem2->type        = Body_Type_Footnote;
	elem2->footnote.id = this->footnotes->len;
}

static def(void, ParseFigure, Body *body, Typography_Node *child) {
	Body *elem = call(Enter, &body->nodes);
	elem->type = Body_Type_Figure;

	call(ParseText, elem, child, NULL);
}

static def(void, ParseCaption, Body *body, Typography_Node *child) {
	Body *elem = call(Enter, &body->nodes);
	elem->type = Body_Type_Caption;

	call(ParseText, elem, child, NULL);
}

static def(void, ParseItem, Body *body, Typography_Node *child) {
	Body_StyleType style;
	Body_BlockType block;

	RdString name = Typography_Item_GetName(child);

	if ((style = scall(ResolveStyle, name)) != Body_Styles_None) {
		call(ParseStyle, body, style, child);
	} else if ((block = scall(ResolveBlock, name)) != Body_BlockType_None) {
		call(ParseBlock, body, block, child);
	} else if (String_Equals(name, $("list"))) {
		call(ParseList, body, child);
	} else if (String_Equals(name, $("p"))) {
		call(ParseParagraph, body, child);
	} else if (String_Equals(name, $("math"))) {
		call(ParseMath, body, child);
	} else if (String_Equals(name, $("url"))) {
		call(ParseUrl, body, child);
	} else if (String_Equals(name, $("command"))) {
		call(ParseCommand, body, child);
	} else if (String_Equals(name, $("code"))) {
		call(ParseCode, body, child);
	} else if (String_Equals(name, $("mail"))) {
		call(ParseMail, body, child);
	} else if (String_Equals(name, $("anchor"))) {
		call(ParseAnchor, body, child);
	} else if (String_Equals(name, $("jump"))) {
		call(ParseJump, body, child);
	} else if (String_Equals(name, $("image"))) {
		call(ParseImage, body, child);
	} else if (String_Equals(name, $("footnote"))) {
		call(ParseFootnote, body, child);
	} else if (String_Equals(name, $("figure"))) {
		call(ParseFigure, body, child);
	} else if (String_Equals(name, $("caption"))) {
		call(ParseCaption, body, child);
	} else {
		String line = Integer_ToString(child->line);
		Logger_Error(this->logger,
			$("line %: '%' not understood."),
			line.rd, name);
		String_Destroy(&line);
	}
}

static def(String, CleanText, RdString value) {
	String text = String_ReplaceAll(value, $("\n"), $(" "));

	while(String_ReplaceAll(&text, $("\t"), $(" ")));
	while(String_ReplaceAll(&text, $("  "), $(" ")));

	return text;
}

static def(void, SetText, Body *body, String text) {
	Body *elem = call(Enter, &body->nodes);
	elem->type       = Body_Type_Text;
	elem->text.value = text;
}

static def(ref(Handler) *, GetHandler, RdString name, ref(Handler) *handlers) {
	for (; handlers->name.len != 0; handlers++) {
		if (String_Equals(name, handlers->name)) {
			return handlers;
		}
	}

	return NULL;
}

static def(void, ParseText, Body *body, Typography_Node *node, ref(Handler) *handlers) {
	Body *elem = body;

	fwd(i, node->len) {
		Typography_Node *child = node->buf[i];

		if (child->type == Typography_NodeType_Text) {
			RdString value = Typography_Text_GetValue(child);

			if (!this->autoDetectParagraphs) {
				String text = call(CleanText, value);

				if (i == 0) {
					String_Copy(&text, String_Trim(text.rd, String_TrimLeft));
				} else if (i == node->len - 1) {
					String_Copy(&text, String_Trim(text.rd, String_TrimRight));
				}

				call(SetText, elem, text);
			} else {
				size_t i = 0;
				RdString line = $("");
				while (String_Split(value, $("\n\n"), &line)) {
					if (String_Trim(line).len == 0) {
						continue;
					}

					String text;

					if (i == 0) {
						text = call(CleanText, line);
					} else {
						elem = call(Enter, &body->nodes);
						elem->type = Body_Type_Paragraph;

						text = call(CleanText, String_Trim(line, String_TrimLeft));
					}

					call(SetText, elem, text);

					i++;
				}
			}
		} else if (child->type == Typography_NodeType_Item) {
			if (handlers == NULL) {
				goto fallback;
			}

			ref(Node) node = {
				.name    = Typography_Item_GetName(child),
				.options = Typography_Item_GetOptions(child),
				.node    = child
			};

			ref(Handler) *handler = call(GetHandler, node.name, handlers);

			if (handler == NULL) {
				goto fallback;
			}

			callback(handler->onNode, &node);

			when (fallback) {
				call(ParseItem, elem, child);
			}
		}
	}
}

static def(RdString, GetMetaValue, RdString name, Typography_Node *node) {
	fwd(i, node->len) {
		Typography_Node *child = node->buf[i];

		if (child->type == Typography_NodeType_Item) {
			if (String_Equals(Typography_Item_GetName(child), name)) {
				return call(GetValue, child);
			}
		}
	}

	return $("");
}

def(RdString, GetMeta, RdString name) {
	Typography_Node *node = Typography_GetRoot(&this->tyo);

	fwd(i, node->len) {
		Typography_Node *child = node->buf[i];

		if (child->type == Typography_NodeType_Item) {
			if (String_Equals(Typography_Item_GetName(child), $("meta"))) {
				return call(GetMetaValue, name, child);
			}
		}
	}

	return $("");
}

def(RdStringArray *, GetMultiMeta, RdString name) {
	RdStringArray *res = RdStringArray_New(0);

	Typography_Node *node = Typography_GetRoot(&this->tyo);

	fwd(i, node->len) {
		Typography_Node *child = node->buf[i];

		if (child->type == Typography_NodeType_Item) {
			if (String_Equals(Typography_Item_GetName(child), $("meta"))) {
				fwd(j, child->len) {
					Typography_Node *child2 = child->buf[j];

					if (child2->type == Typography_NodeType_Item) {
						if (String_Equals(Typography_Item_GetName(child2), name)) {
							RdStringArray_Push(&res,
								call(GetValue, child2));
						}
					}
				}

				break;
			}
		}
	}

	return res;
}

def(void, ProcessNodes, Typography_Node *node, ref(Handler) *handlers) {
	fwd(i, node->len) {
		Typography_Node *child = node->buf[i];

		if (child->type == Typography_NodeType_Item) {
			ref(Node) node = {
				.name    = Typography_Item_GetName(child),
				.options = Typography_Item_GetOptions(child),
				.node    = child
			};

			ref(Handler) *handler = call(GetHandler, node.name, handlers);

			if (handler == NULL) {
				String line = Integer_ToString(child->line);
				Logger_Error(this->logger,
					$("line %: '%' not understood."),
					line.rd, node.name);
				String_Destroy(&line);

				continue;
			}

			callback(handler->onNode, &node);
		}
	}
}

def(Body, ProcessBody, Typography_Node *node, ref(Handler) *handlers) {
	Body body = {
		.type  = Body_Type_Collection,
		.nodes = BodyArray_New(Body_DefaultLength)
	};

	call(ParseText, &body, node, handlers);

	return body;
}

def(ref(Node), GetNodeByName, RdString name) {
	Typography_Node *node = Typography_GetRoot(&this->tyo);

	fwd(i, node->len) {
		Typography_Node *child = node->buf[i];

		if (child->type == Typography_NodeType_Item) {
			if (String_Equals(Typography_Item_GetName(child), name)) {
				return (ref(Node)) {
					.name    = name,
					.options = Typography_Item_GetOptions(child),
					.node    = child
				};
			}
		}
	}

	return (ref(Node)) {
		.name    = $(""),
		.options = $(""),
		.node    = NULL
	};
}
