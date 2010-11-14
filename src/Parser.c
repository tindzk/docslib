#import "Parser.h"
#import <App.h>

extern Logger logger;

static struct {
	String     name;
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
	String         name;
	Body_BlockType block;
} blocks[] = {
	{ $("note"),    Body_BlockType_Note    },
	{ $("warning"), Body_BlockType_Warning }
};

Body_Style ref(ResolveStyle)(String name) {
	for (size_t i = 0; i < nElems(styles); i++) {
		if (String_Equals(name, styles[i].name)) {
			return styles[i].style;
		}
	}

	return Body_Styles_None;
}

Body_BlockType ref(ResolveBlock)(String name) {
	for (size_t i = 0; i < nElems(blocks); i++) {
		if (String_Equals(name, blocks[i].name)) {
			return blocks[i].block;
		}
	}

	return Body_BlockType_None;
}

def(void, Init) {
	Typography_Init(&this->tyo);
}

def(void, Parse, String path) {
	File file;
	File_Open(&file, path, FileStatus_ReadOnly);

	BufferedStream stream;
	BufferedStream_Init(&stream, &FileStreamImpl, &file);
	BufferedStream_SetInputBuffer(&stream, 1024, 128);

	Typography_Parse(&this->tyo, &BufferedStreamImpl, &stream);

	BufferedStream_Close(&stream);
	BufferedStream_Destroy(&stream);
}

def(void, Destroy) {
	Typography_Destroy(&this->tyo);
}

def(Typography_Node *, GetRoot) {
	return Typography_GetRoot(&this->tyo);
}

static def(String, GetValue, Typography_Node *node) {
	if (node->len == 0) {
		Logger_Error(&logger, $("line %: value expected."),
			Integer_ToString(node->line));

		return $("");
	}

	if (node->len > 1) {
		Logger_Error(&logger, $("line %: too many nodes."),
			Integer_ToString(node->line));

		return $("");
	}

	Typography_Node *child = node->buf[0];

	if (child->type != Typography_NodeType_Text) {
		Logger_Error(&logger,
			$("line %: node given, text expected."),
			Integer_ToString(node->line));

		return $("");
	}

	return Typography_Text(child)->value;
}

static def(Body *, Enter, Body *parent) {
	Body *body = New(Body);

	body->type  = Body_Type_Collection;
	body->nodes = BodyArray_New(Body_DefaultLength);

	BodyArray_Push(&parent->nodes, body);

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

static def(void, SetList, Body *body) {
	body->type = Body_Type_List;
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
	Body *list = call(Enter, body);
	call(SetList, list);

	for (size_t i = 0; i < node->len; i++) {
		Typography_Node *child = node->buf[i];

		if (child->type == Typography_NodeType_Item) {
			if (!String_Equals(Typography_Item(child)->name, $("item"))) {
				Logger_Error(&logger,
					$("line %: got '%', 'item' expected."),
					Integer_ToString(child->line),
					Typography_Item(child)->name);

				continue;
			}

			Body *listItem = call(Enter, list);
			call(SetListItem, listItem);

			call(ParseStyleBlock, listItem, child, 0);
		}
	}
}

static def(String, CleanValue, String value) {
	String out = value;

	ssize_t pos = String_Find(value, '\n');

	if (pos != String_NotFound) {
		String firstLine = String_Slice(value, 0, pos);

		/* Skip first line if empty. */
		if (String_Trim(firstLine).len == 0) {
			out = String_Slice(value, pos + 1);
		}
	}

	return String_Trim(out, String_TrimRight);
}

static def(void, ParseCommand, Body *body, Typography_Node *child) {
	String value   = call(GetValue,   child);
	String cleaned = call(CleanValue, value);

	Body *cmd = call(Enter, body);
	call(SetCommand, cmd, String_Clone(cleaned));
}

static def(void, ParseCode, Body *body, Typography_Node *child) {
	String value   = call(GetValue,   child);
	String cleaned = call(CleanValue, value);

	Body *code = call(Enter, body);
	call(SetCode, code, String_Clone(cleaned));
}

static def(void, ParseMail, Body *body, Typography_Node *child) {
	String addr = String_Trim(Typography_Item(child)->options);

	Body *mail = call(Enter, body);
	call(SetMail, mail, String_Clone(addr));

	call(ParseStyleBlock, mail, child, 0);
}

static def(void, ParseAnchor, Body *body, Typography_Node *child) {
	String value = String_Trim(call(GetValue, child));

	Body *anchor = call(Enter, body);
	call(SetAnchor, anchor, String_Clone(value));
}

static def(void, ParseJump, Body *body, Typography_Node *child) {
	String anchor = String_Trim(Typography_Item(child)->options);

	Body *jump = call(Enter, body);
	call(SetJump, jump, String_Clone(anchor));

	call(ParseStyleBlock, jump, child, 0);
}

static def(void, ParseUrl, Body *body, Typography_Node *child) {
	String url = String_Trim(Typography_Item(child)->options);

	Body *elem = call(Enter, body);
	call(SetUrl, elem, String_Clone(url));

	call(ParseStyleBlock, elem, child, 0);
}

static def(void, ParseImage, Body *body, Typography_Node *child) {
	String path = String_Trim(call(GetValue, child));

	Body *image = call(Enter, body);
	call(SetImage, image, String_Clone(path));
}

static def(void, ParseParagraph, Body *body, Typography_Node *child) {
	Body *parag = call(Enter, body);
	call(SetParagraph, parag);

	call(ParseStyleBlock, parag, child, 0);
}

static def(void, ParseBlock, Body *body, Body_BlockType type, Typography_Node *child) {
	Body *block = call(Enter, body);
	call(SetBlock, block, type);

	call(ParseStyleBlock, block, child, 0);
}

static def(void, ParseItem, Body *body, Typography_Node *child, int style) {
	Body_Style _style;
	Body_BlockType type;

	String name = Typography_Item(child)->name;

	if ((_style = ref(ResolveStyle)(name)) != Body_Styles_None) {
		BitMask_Set(style, _style);
		call(ParseStyleBlock, body, child, style);
	} else if ((type = ref(ResolveBlock)(name)) != Body_BlockType_None) {
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
	} else {
		Logger_Error(&logger,
			$("line %: '%' not understood."),
			Integer_ToString(child->line),
			Typography_Item(child)->name);
	}
}

static def(void, AddText, Body *body, String text, int style) {
	if (style != 0) {
		Body *elem = call(Enter, body);
		call(SetText, elem, text, style);

		return;
	}

	size_t last = 0;

	for (;;) {
		ssize_t pos = String_Find(text, last, $("\n\n"));

		if (pos == String_NotFound) {
			String_Crop(&text, last);

			Body *elem = call(Enter, body);
			call(SetText, elem, text, 0);

			break;
		} else {
			String_Crop(&text, last, pos - last);

			Body *elem = call(Enter, body);
			call(SetText, elem, text, 0);
		}

		last = pos + 1;
	}
}

static def(String, CleanText, String value) {
	String text = String_ReplaceAll(value, $("\n"), $(" "));

	while(String_ReplaceAll(&text, $("\t"), $(" ")));
	while(String_ReplaceAll(&text, $("  "), $(" ")));

	return text;
}

static def(void, ParseStyleBlock, Body *body, Typography_Node *node, int style) {
	for (size_t i = 0; i < node->len; i++) {
		Typography_Node *child = node->buf[i];

		if (child->type == Typography_NodeType_Text) {
			String text = call(CleanText,
				Typography_Text(child)->value);

			if (i == 0) {
				String_Trim(&text, String_TrimLeft);
			} else if (i == node->len - 1) {
				String_Trim(&text, String_TrimRight);
			}

			call(AddText, body, text, style);
		} else if (child->type == Typography_NodeType_Item) {
			call(ParseItem, body, child, style);
		}
	}
}

static def(String, GetMetaValue, String name, Typography_Node *node) {
	for (size_t i = 0; i < node->len; i++) {
		Typography_Node *child = node->buf[i];

		if (child->type == Typography_NodeType_Item) {
			if (String_Equals(Typography_Item(child)->name, name)) {
				return String_Disown(call(GetValue, child));
			}
		}
	}

	return $("");
}

def(String, GetMeta, String name) {
	Typography_Node *node = Typography_GetRoot(&this->tyo);

	for (size_t i = 0; i < node->len; i++) {
		Typography_Node *child = node->buf[i];

		if (child->type == Typography_NodeType_Item) {
			if (String_Equals(Typography_Item(child)->name, $("meta"))) {
				return call(GetMetaValue, name, child);
			}
		}
	}

	return $("");
}

def(Body, GetBody, Typography_Node *node, String ignore) {
	Body body = {
		.type  = Body_Type_Collection,
		.nodes = BodyArray_New(Body_DefaultLength)
	};

	for (size_t i = 0; i < node->len; i++) {
		Typography_Node *child = node->buf[i];

		if (child->type == Typography_NodeType_Text) {
			String text = call(CleanText,
				Typography_Text(child)->value);

			if (i == 0) {
				String_Trim(&text, String_TrimLeft);
			} else if (i == node->len - 1) {
				String_Trim(&text, String_TrimRight);
			}

			call(AddText, &body, text, 0);
		} else if (child->type == Typography_NodeType_Item) {
			if (String_Equals(Typography_Item(child)->name, ignore)) {
				continue;
			}

			call(ParseItem, &body, child, 0);
		}
	}

	return body;
}

def(ref(Nodes) *, GetNodes, Typography_Node *node) {
	ref(Nodes) *res = scall(Nodes_New, 0);

	for (size_t i = 0; i < node->len; i++) {
		Typography_Node *child = node->buf[i];

		if (child->type == Typography_NodeType_Item) {
			ref(Node) node = {
				.name    = String_Disown(Typography_Item(child)->name),
				.options = String_Disown(Typography_Item(child)->options),
				.node    = child
			};

			scall(Nodes_Push, &res, node);
		}
	}

	return res;
}

def(ref(Node), GetNodeByName, String name) {
	Typography_Node *node = Typography_GetRoot(&this->tyo);

	for (size_t i = 0; i < node->len; i++) {
		Typography_Node *child = node->buf[i];

		if (child->type == Typography_NodeType_Item) {
			if (String_Equals(Typography_Item(child)->name, name)) {
				return (ref(Node)) {
					.name    = name,
					.options = String_Disown(Typography_Item(child)->options),
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
