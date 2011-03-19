#import "Parser.h"

#define self Parser

static struct {
	RdString   name;
	Body_Style style;
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

static sdef(Body_Style, ResolveStyle, RdString name) {
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

	each(fn, this->footnotes) {
		Body_Destroy(*fn);
		Body_Free(*fn);
	}

	BodyArray_Free(this->footnotes);
}

def(BodyArray *, GetFootnotes) {
	return this->footnotes;
}

def(Typography_Node *, GetRoot) {
	return Typography_GetRoot(&this->tyo);
}

def(void, Parse, RdString path) {
	File file;
	File_Open(&file, path, FileStatus_ReadOnly);

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
	Body *body = Body_Alloc();

	body->type  = Body_Type_Collection;
	body->nodes = BodyArray_New(Body_DefaultLength);

	BodyArray_Push(arr, body);

	return body;
}

static def(void, ParseStyleBlock, Body *body, Typography_Node *node, int style);

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

			call(ParseStyleBlock, listItem, child, 0);
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

static def(void, ParseMail, Body *body, Typography_Node *child) {
	RdString addr = String_Trim(Typography_Item_GetOptions(child));

	Body *elem = call(Enter, &body->nodes);
	elem->type      = Body_Type_Mail;
	elem->mail.addr = String_Clone(addr);

	call(ParseStyleBlock, elem, child, 0);
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

	call(ParseStyleBlock, elem, child, 0);
}

static def(void, ParseUrl, Body *body, Typography_Node *child) {
	RdString url = String_Trim(Typography_Item_GetOptions(child));

	Body *elem = call(Enter, &body->nodes);
	elem->type    = Body_Type_Url;
	elem->url.url = String_Clone(url);

	call(ParseStyleBlock, elem, child, 0);
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

	call(ParseStyleBlock, elem, child, 0);
}

static def(void, ParseBlock, Body *body, Body_BlockType type, Typography_Node *child) {
	Body *elem = call(Enter, &body->nodes);
	elem->type       = Body_Type_Block;
	elem->block.type = type;

	call(ParseStyleBlock, elem, child, 0);
}

static def(void, ParseFootnote, Body *body, Typography_Node *child) {
	Body *elem = call(Enter, &this->footnotes);
	call(ParseStyleBlock, elem, child, 0);

	Body *elem2 = call(Enter, &body->nodes);
	elem2->type        = Body_Type_Footnote;
	elem2->footnote.id = this->footnotes->len;
}

static def(void, ParseFigure, Body *body, Typography_Node *child) {
	Body *elem = call(Enter, &body->nodes);
	elem->type = Body_Type_Figure;

	call(ParseStyleBlock, elem, child, 0);
}

static def(void, ParseCaption, Body *body, Typography_Node *child) {
	Body *elem = call(Enter, &body->nodes);
	elem->type = Body_Type_Caption;

	call(ParseStyleBlock, elem, child, 0);
}

static def(void, ParseItem, Body *body, Typography_Node *child, int style) {
	Body_Style _style;
	Body_BlockType type;

	RdString name = Typography_Item_GetName(child);

	if ((_style = scall(ResolveStyle, name)) != Body_Styles_None) {
		BitMask_Set(style, _style);
		call(ParseStyleBlock, body, child, style);
	} else if ((type = scall(ResolveBlock, name)) != Body_BlockType_None) {
		call(ParseBlock, body, type, child);
	} else if (String_Equals(name, $("list"))) {
		call(ParseList, body, child);
	} else if (String_Equals(name, $("p"))) {
		call(ParseParagraph, body, child);
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

static def(void, SetText, Body *body, String text, int style) {
	body->type       = Body_Type_Text;
	body->text.value = text;
	body->text.style = style;
}

static def(void, AddText, Body *body, String text, int style) {
	if (style != 0) {
		Body *elem = call(Enter, &body->nodes);
		call(SetText, elem, text, style);

		return;
	}

	size_t last = 0;

	for (;;) {
		ssize_t pos = String_Find(text.rd, last, $("\n\n"));

		if (pos == String_NotFound) {
			String_Crop(&text, last);

			Body *elem = call(Enter, &body->nodes);
			call(SetText, elem, text, 0);

			break;
		} else {
			String_Crop(&text, last, pos - last);

			Body *elem = call(Enter, &body->nodes);
			call(SetText, elem, text, 0);
		}

		last = pos + 1;
	}
}

static def(String, CleanText, RdString value) {
	String text = String_ReplaceAll(value, $("\n"), $(" "));

	while(String_ReplaceAll(&text, $("\t"), $(" ")));
	while(String_ReplaceAll(&text, $("  "), $(" ")));

	return text;
}

static def(void, ParseStyleBlock, Body *body, Typography_Node *node, int style) {
	fwd(i, node->len) {
		Typography_Node *child = node->buf[i];

		if (child->type == Typography_NodeType_Text) {
			String text = call(CleanText, Typography_Text_GetValue(child));

			if (i == 0) {
				String_Copy(&text, String_Trim(text.rd, String_TrimLeft));
			} else if (i == node->len - 1) {
				String_Copy(&text, String_Trim(text.rd, String_TrimRight));
			}

			call(AddText, body, text, style);
		} else if (child->type == Typography_NodeType_Item) {
			call(ParseItem, body, child, style);
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

def(Body, GetBody, Typography_Node *node, RdString ignore) {
	Body body = {
		.type  = Body_Type_Collection,
		.nodes = BodyArray_New(Body_DefaultLength)
	};

	fwd(i, node->len) {
		Typography_Node *child = node->buf[i];

		if (child->type == Typography_NodeType_Text) {
			String text = call(CleanText, Typography_Text_GetValue(child));

			if (i == 0) {
				String_Copy(&text, String_Trim(text.rd, String_TrimLeft));
			} else if (i == node->len - 1) {
				String_Copy(&text, String_Trim(text.rd, String_TrimRight));
			}

			call(AddText, &body, text, 0);
		} else if (child->type == Typography_NodeType_Item) {
			if (String_Equals(Typography_Item_GetName(child), ignore)) {
				continue;
			}

			call(ParseItem, &body, child, 0);
		}
	}

	return body;
}

def(ref(Nodes) *, GetNodes, Typography_Node *node) {
	ref(Nodes) *res = scall(Nodes_New, 0);

	fwd(i, node->len) {
		Typography_Node *child = node->buf[i];

		if (child->type == Typography_NodeType_Item) {
			ref(Node) node = {
				.name    = Typography_Item_GetName(child),
				.options = Typography_Item_GetOptions(child),
				.node    = child
			};

			scall(Nodes_Push, &res, node);
		}
	}

	return res;
}

def(ref(Nodes) *, GetNodesByName, Typography_Node *node, RdString name) {
	ref(Nodes) *res = scall(Nodes_New, 0);

	fwd(i, node->len) {
		Typography_Node *child = node->buf[i];

		if (child->type == Typography_NodeType_Item) {
			if (!String_Equals(Typography_Item_GetName(child), name)) {
				continue;
			}

			ref(Node) node = {
				.name    = Typography_Item_GetName(child),
				.options = Typography_Item_GetOptions(child),
				.node    = child
			};

			scall(Nodes_Push, &res, node);
		}
	}

	return res;
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
