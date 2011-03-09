#import "Parser.h"

#define self Parser

extern Logger logger;

static struct {
	ProtString name;
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
	ProtString     name;
	Body_BlockType block;
} blocks[] = {
	{ $("note"),    Body_BlockType_Note    },
	{ $("warning"), Body_BlockType_Warning },
	{ $("quote"),   Body_BlockType_Quote   }
};

static sdef(Body_Style, ResolveStyle, ProtString name) {
	forward (i, nElems(styles)) {
		if (String_Equals(name, styles[i].name)) {
			return styles[i].style;
		}
	}

	return Body_Styles_None;
}

static sdef(Body_BlockType, ResolveBlock, ProtString name) {
	forward (i, nElems(blocks)) {
		if (String_Equals(name, blocks[i].name)) {
			return blocks[i].block;
		}
	}

	return Body_BlockType_None;
}

rsdef(self, New) {
	return (self) {
		.tyo = Typography_New(),
		.footnotes = BodyArray_New(1024)
	};
}

def(void, Destroy) {
	Typography_Destroy(&this->tyo);

	foreach (fn, this->footnotes) {
		Body_Destroy(*fn);
		Pool_Free(Pool_GetInstance(), *fn);
	}

	BodyArray_Free(this->footnotes);
}

def(BodyArray *, GetFootnotes) {
	return this->footnotes;
}

def(Typography_Node *, GetRoot) {
	return Typography_GetRoot(&this->tyo);
}

def(void, Parse, ProtString path) {
	File file;
	File_Open(&file, path, FileStatus_ReadOnly);

	BufferedStream stream = BufferedStream_New(File_AsStream(&file));
	BufferedStream_SetInputBuffer(&stream, 1024, 128);

	Typography_Parse(&this->tyo, BufferedStream_AsStream(&stream));

	BufferedStream_Close(&stream);
	BufferedStream_Destroy(&stream);
}

static def(ProtString, GetValue, Typography_Node *node) {
	if (node->len == 0) {
		String line = Integer_ToString(node->line);
		Logger_Error(&logger, $("line %: value expected."), line.prot);
		String_Destroy(&line);

		return $("");
	}

	if (node->len > 1) {
		String line = Integer_ToString(node->line);
		Logger_Error(&logger, $("line %: too many nodes."), line.prot);
		String_Destroy(&line);

		return $("");
	}

	Typography_Node *child = node->buf[0];

	if (child->type != Typography_NodeType_Text) {
		String line = Integer_ToString(child->line);
		Logger_Error(&logger,
			$("line %: node given, text expected."),
			line.prot);
		String_Destroy(&line);

		return $("");
	}

	return Typography_Text(child)->value.prot;
}

static def(Body *, Enter, BodyArray **arr) {
	Body *body = Pool_Alloc(Pool_GetInstance(), sizeof(Body));

	body->type  = Body_Type_Collection;
	body->nodes = BodyArray_New(Body_DefaultLength);

	BodyArray_Push(arr, body);

	return body;
}

static def(void, SetBlock, Body *body, Body_BlockType type) {
	body->type       = Body_Type_Block;
	body->block.type = type;
}

static def(void, SetParagraph, Body *body) {
	body->type = Body_Type_Paragraph;
}

static def(void, SetUrl, Body *body, String url) {
	body->type    = Body_Type_Url;
	body->url.url = url;
}

static def(void, SetList, Body *body, bool ordered) {
	body->type = Body_Type_List;
	body->list.ordered = ordered;
}

static def(void, SetListItem, Body *body) {
	body->type = Body_Type_ListItem;
}

static def(void, SetText, Body *body, String text, int style) {
	body->type       = Body_Type_Text;
	body->text.value = text;
	body->text.style = style;
}

static def(void, SetCommand, Body *body, String value) {
	body->type          = Body_Type_Command;
	body->command.value = value;
}

static def(void, SetCode, Body *body, String value) {
	body->type       = Body_Type_Code;
	body->code.value = value;
}

static def(void, SetMail, Body *body, String addr) {
	body->type      = Body_Type_Mail;
	body->mail.addr = addr;
}

static def(void, SetImage, Body *body, String path) {
	body->type       = Body_Type_Image;
	body->image.path = path;
}

static def(void, SetAnchor, Body *body, String name) {
	body->type        = Body_Type_Anchor;
	body->anchor.name = name;
}

static def(void, SetJump, Body *body, String anchor) {
	body->type        = Body_Type_Jump;
	body->jump.anchor = anchor;
}

static def(void, ParseStyleBlock, Body *body, Typography_Node *node, int style);

static def(void, ParseList, Body *body, Typography_Node *node) {
	ProtString options = String_Trim(Typography_Item(node)->options.prot);
	bool ordered = String_Equals(options, $("ordered"));

	Body *list = call(Enter, &body->nodes);
	call(SetList, list, ordered);

	forward (i, node->len) {
		Typography_Node *child = node->buf[i];

		if (child->type == Typography_NodeType_Item) {
			if (!String_Equals(Typography_Item(child)->name.prot, $("item"))) {
				String line = Integer_ToString(child->line);
				Logger_Error(&logger,
					$("line %: got '%', 'item' expected."),
					line.prot, Typography_Item(child)->name.prot);
				String_Destroy(&line);

				continue;
			}

			Body *listItem = call(Enter, &list->nodes);
			call(SetListItem, listItem);

			call(ParseStyleBlock, listItem, child, 0);
		}
	}
}

static def(String, CleanValue, ProtString value) {
	ssize_t pos = String_Find(value, '\n');

	if (pos != String_NotFound) {
		ProtString firstLine = String_Slice(value, 0, pos);

		/* Skip first line if empty. */
		if (String_Trim(firstLine).len == 0) {
			value = String_Slice(value, pos + 1);
		}
	}

	value = String_Trim(value, String_TrimRight);

	ProtStringArray *lines = String_Split(value, '\n');

	CarrierStringArray *dest = CarrierStringArray_New(lines->len);

	ssize_t unindent = -1;

	foreach (line, lines) {
		CarrierStringArray_Push(&dest, String_ToCarrier(*line));

		if (line->len == 0) {
			continue;
		}

		size_t tabs = 0;

		forward (i, line->len) {
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

	ProtStringArray_Free(lines);

	foreach (line, dest) {
		if (line->len > 0) {
			line->prot = String_Slice(line->prot, unindent);

			/* Replace all leading tabs by 4 spaces as there is no
			 * `tab-stops' property in CSS.
			 * See also http://www.pixelastic.com/blog/79:setting-the-size-of-a-tab-character-in-a-element
			 */

			size_t tabs = 0;

			forward (i, line->len) {
				if (line->buf[i] == '\t') {
					tabs++;
				} else {
					break;
				}
			}

			if (tabs > 0) {
				String new = String_New(line->len - tabs + tabs * 4);

				repeat (tabs * 4) {
					new.buf[new.len] = ' ';
					new.len++;
				}

				String_Append(&new, String_Slice(line->prot, tabs));

				CarrierString_Assign(line, String_ToCarrier(new));
			}
		}
	}

	ProtStringArray *copy = ProtStringArray_New(dest->len);

	forward (i, dest->len) {
		copy->buf[i] = dest->buf[i].prot;
	}

	copy->len = dest->len;

	String res = ProtStringArray_Join(copy, $("\n"));

	ProtStringArray_Free(copy);

	/* Free all lines in which the tabs were replaced. */
	CarrierStringArray_Destroy(dest);
	CarrierStringArray_Free(dest);

	return res;
}

static def(void, ParseCommand, Body *body, Typography_Node *child) {
	ProtString value = call(GetValue,   child);
	String cleaned   = call(CleanValue, value);

	Body *cmd = call(Enter, &body->nodes);
	call(SetCommand, cmd, cleaned);
}

static def(void, ParseCode, Body *body, Typography_Node *child) {
	ProtString value = call(GetValue,   child);
	String cleaned   = call(CleanValue, value);

	Body *code = call(Enter, &body->nodes);
	call(SetCode, code, cleaned);
}

static def(void, ParseMail, Body *body, Typography_Node *child) {
	ProtString addr = String_Trim(Typography_Item(child)->options.prot);

	Body *mail = call(Enter, &body->nodes);
	call(SetMail, mail, String_Clone(addr));

	call(ParseStyleBlock, mail, child, 0);
}

static def(void, ParseAnchor, Body *body, Typography_Node *child) {
	ProtString value = String_Trim(call(GetValue, child));

	Body *anchor = call(Enter, &body->nodes);
	call(SetAnchor, anchor, String_Clone(value));
}

static def(void, ParseJump, Body *body, Typography_Node *child) {
	ProtString anchor = String_Trim(Typography_Item(child)->options.prot);

	Body *jump = call(Enter, &body->nodes);
	call(SetJump, jump, String_Clone(anchor));

	call(ParseStyleBlock, jump, child, 0);
}

static def(void, ParseUrl, Body *body, Typography_Node *child) {
	ProtString url = String_Trim(Typography_Item(child)->options.prot);

	Body *elem = call(Enter, &body->nodes);
	call(SetUrl, elem, String_Clone(url));

	call(ParseStyleBlock, elem, child, 0);
}

static def(void, ParseImage, Body *body, Typography_Node *child) {
	ProtString path = String_Trim(call(GetValue, child));

	Body *image = call(Enter, &body->nodes);
	call(SetImage, image, String_Clone(path));
}

static def(void, ParseParagraph, Body *body, Typography_Node *child) {
	Body *parag = call(Enter, &body->nodes);
	call(SetParagraph, parag);

	call(ParseStyleBlock, parag, child, 0);
}

static def(void, ParseBlock, Body *body, Body_BlockType type, Typography_Node *child) {
	Body *block = call(Enter, &body->nodes);
	call(SetBlock, block, type);

	call(ParseStyleBlock, block, child, 0);
}

static def(void, ParseFootnote, Body *body, Typography_Node *child) {
	Body *fn = call(Enter, &this->footnotes);
	call(ParseStyleBlock, fn, child, 0);

	Body *fn2 = call(Enter, &body->nodes);
	fn2->type = Body_Type_Footnote;
	fn2->footnote.id = this->footnotes->len;
}

static def(void, ParseItem, Body *body, Typography_Node *child, int style) {
	Body_Style _style;
	Body_BlockType type;

	ProtString name = Typography_Item(child)->name.prot;

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
	} else {
		String line = Integer_ToString(child->line);
		Logger_Error(&logger,
			$("line %: '%' not understood."),
			line.prot, Typography_Item(child)->name.prot);
		String_Destroy(&line);
	}
}

static def(void, AddText, Body *body, String text, int style) {
	if (style != 0) {
		Body *elem = call(Enter, &body->nodes);
		call(SetText, elem, text, style);

		return;
	}

	size_t last = 0;

	for (;;) {
		ssize_t pos = String_Find(text.prot, last, $("\n\n"));

		if (pos == String_NotFound) {
			String_FastCrop(&text, last);

			Body *elem = call(Enter, &body->nodes);
			call(SetText, elem, text, 0);

			break;
		} else {
			String_FastCrop(&text, last, pos - last);

			Body *elem = call(Enter, &body->nodes);
			call(SetText, elem, text, 0);
		}

		last = pos + 1;
	}
}

static def(String, CleanText, ProtString value) {
	String text = String_ReplaceAll(value, $("\n"), $(" "));

	while(String_ReplaceAll(&text, $("\t"), $(" ")));
	while(String_ReplaceAll(&text, $("  "), $(" ")));

	return text;
}

static def(void, ParseStyleBlock, Body *body, Typography_Node *node, int style) {
	forward (i, node->len) {
		Typography_Node *child = node->buf[i];

		if (child->type == Typography_NodeType_Text) {
			String text = call(CleanText,
				Typography_Text(child)->value.prot);

			if (i == 0) {
				String_Copy(&text, String_Trim(text.prot, String_TrimLeft));
			} else if (i == node->len - 1) {
				String_Copy(&text, String_Trim(text.prot, String_TrimRight));
			}

			call(AddText, body, text, style);
		} else if (child->type == Typography_NodeType_Item) {
			call(ParseItem, body, child, style);
		}
	}
}

static def(ProtString, GetMetaValue, ProtString name, Typography_Node *node) {
	forward (i, node->len) {
		Typography_Node *child = node->buf[i];

		if (child->type == Typography_NodeType_Item) {
			if (String_Equals(Typography_Item(child)->name.prot, name)) {
				return call(GetValue, child);
			}
		}
	}

	return $("");
}

def(ProtString, GetMeta, ProtString name) {
	Typography_Node *node = Typography_GetRoot(&this->tyo);

	forward (i, node->len) {
		Typography_Node *child = node->buf[i];

		if (child->type == Typography_NodeType_Item) {
			if (String_Equals(Typography_Item(child)->name.prot, $("meta"))) {
				return call(GetMetaValue, name, child);
			}
		}
	}

	return $("");
}

def(ProtStringArray *, GetMultiMeta, ProtString name) {
	ProtStringArray *res = ProtStringArray_New(0);

	Typography_Node *node = Typography_GetRoot(&this->tyo);

	forward (i, node->len) {
		Typography_Node *child = node->buf[i];

		if (child->type == Typography_NodeType_Item) {
			if (String_Equals(Typography_Item(child)->name.prot, $("meta"))) {
				forward (j, child->len) {
					Typography_Node *child2 = child->buf[j];

					if (child2->type == Typography_NodeType_Item) {
						if (String_Equals(Typography_Item(child2)->name.prot, name)) {
							ProtStringArray_Push(&res,
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

def(Body, GetBody, Typography_Node *node, ProtString ignore) {
	Body body = {
		.type  = Body_Type_Collection,
		.nodes = BodyArray_New(Body_DefaultLength)
	};

	forward(i, node->len) {
		Typography_Node *child = node->buf[i];

		if (child->type == Typography_NodeType_Text) {
			String text = call(CleanText,
				Typography_Text(child)->value.prot);

			if (i == 0) {
				String_Copy(&text, String_Trim(text.prot, String_TrimLeft));
			} else if (i == node->len - 1) {
				String_Copy(&text, String_Trim(text.prot, String_TrimRight));
			}

			call(AddText, &body, text, 0);
		} else if (child->type == Typography_NodeType_Item) {
			if (String_Equals(Typography_Item(child)->name.prot, ignore)) {
				continue;
			}

			call(ParseItem, &body, child, 0);
		}
	}

	return body;
}

def(ref(Nodes) *, GetNodes, Typography_Node *node) {
	ref(Nodes) *res = scall(Nodes_New, 0);

	forward (i, node->len) {
		Typography_Node *child = node->buf[i];

		if (child->type == Typography_NodeType_Item) {
			ref(Node) node = {
				.name    = Typography_Item(child)->name.prot,
				.options = Typography_Item(child)->options.prot,
				.node    = child
			};

			scall(Nodes_Push, &res, node);
		}
	}

	return res;
}

def(ref(Nodes) *, GetNodesByName, Typography_Node *node, ProtString name) {
	ref(Nodes) *res = scall(Nodes_New, 0);

	forward (i, node->len) {
		Typography_Node *child = node->buf[i];

		if (child->type == Typography_NodeType_Item) {
			if (!String_Equals(Typography_Item(child)->name.prot, name)) {
				continue;
			}

			ref(Node) node = {
				.name    = Typography_Item(child)->name.prot,
				.options = Typography_Item(child)->options.prot,
				.node    = child
			};

			scall(Nodes_Push, &res, node);
		}
	}

	return res;
}

def(ref(Node), GetNodeByName, ProtString name) {
	Typography_Node *node = Typography_GetRoot(&this->tyo);

	forward (i, node->len) {
		Typography_Node *child = node->buf[i];

		if (child->type == Typography_NodeType_Item) {
			if (String_Equals(Typography_Item(child)->name.prot, name)) {
				return (ref(Node)) {
					.name    = name,
					.options = Typography_Item(child)->options.prot,
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
