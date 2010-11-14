#import <Bit.h>
#import <String.h>

#undef self
#define self Body

#ifndef Body_DefaultLength
#define Body_DefaultLength 128
#endif

set(ref(Style)) {
	ref(Styles_None)     = 0,
	ref(Styles_Bold)     = Bit(0),
	ref(Styles_Italic)   = Bit(1),
	ref(Styles_Class)    = Bit(2),
	ref(Styles_Function) = Bit(3),
	ref(Styles_Variable) = Bit(4),
	ref(Styles_Macro)    = Bit(5),
	ref(Styles_Term)     = Bit(6),
	ref(Styles_Keyword)  = Bit(7),
	ref(Styles_Path)     = Bit(8),
	ref(Styles_Number)   = Bit(9)
};

set(ref(BlockType)) {
	ref(BlockType_None),
	ref(BlockType_Note),
	ref(BlockType_Warning)
};

class(ref(Text)) {
	String value;
	int style;
};

class(ref(Block)) {
	ref(BlockType) type;
};

class(ref(Image)) {
	String path;
};

class(ref(Command)) {
	String value;
};

class(ref(Code)) {
	String value;
};

class(ref(Mail)) {
	String addr;
};

class(ref(Anchor)) {
	String name;
};

class(ref(Jump)) {
	String anchor;
};

class(ref(Url)) {
	String url;
};

set(ref(Type)) {
	ref(Type_Collection),
	ref(Type_Block),
	ref(Type_Paragraph),
	ref(Type_List),
	ref(Type_ListItem),
	ref(Type_Url),
	ref(Type_Text),
	ref(Type_Image),
	ref(Type_Command),
	ref(Type_Code),
	ref(Type_Mail),
	ref(Type_Anchor),
	ref(Type_Jump),
	ref(Type_Empty)
};

Array_Define(struct self *, BodyArray);

class(self) {
	ref(Type) type;

	union {
		ref(Url)     url;
		ref(Code)    code;
		ref(Mail)    mail;
		ref(Text)    text;
		ref(Jump)    jump;
		ref(Image)   image;
		ref(Block)   block;
		ref(Anchor)  anchor;
		ref(Command) command;
	};

	BodyArray *nodes;
};

sdef(self, Empty);
def(void, Destroy);
