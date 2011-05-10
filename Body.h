#import <Bit.h>
#import <String.h>

#define self Body

#ifndef Body_DefaultLength
#define Body_DefaultLength 128
#endif

set(ref(StyleType)) {
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
	ref(BlockType_Warning),
	ref(BlockType_Quote)
};

record(ref(Text)) {
	String value;
};

record(ref(Style)) {
	ref(StyleType) type;
};

record(ref(Block)) {
	ref(BlockType) type;
};

record(ref(Image)) {
	String path;
};

record(ref(Command)) {
	String value;
};

record(ref(Code)) {
	String value;
};

record(ref(Math)) {
	String value;
};

record(ref(Mail)) {
	String addr;
};

record(ref(Anchor)) {
	String name;
};

record(ref(Jump)) {
	String anchor;
};

record(ref(List)) {
	bool ordered;
};

record(ref(Url)) {
	String url;
};

record(ref(Footnote)) {
	size_t id;
};

set(ref(Type)) {
	ref(Type_Collection),
	ref(Type_Block),
	ref(Type_Paragraph),
	ref(Type_List),
	ref(Type_ListItem),
	ref(Type_Url),
	ref(Type_Text),
	ref(Type_Style),
	ref(Type_Image),
	ref(Type_Math),
	ref(Type_Command),
	ref(Type_Footnote),
	ref(Type_Code),
	ref(Type_Mail),
	ref(Type_Anchor),
	ref(Type_Jump),
	ref(Type_Figure),
	ref(Type_Caption),
	ref(Type_Empty)
};

Array(struct self *, BodyArray);

class {
	ref(Type) type;

	union {
		ref(Url)      url;
		ref(List)     list;
		ref(Code)     code;
		ref(Math)     math;
		ref(Mail)     mail;
		ref(Text)     text;
		ref(Jump)     jump;
		ref(Style)    style;
		ref(Image)    image;
		ref(Block)    block;
		ref(Anchor)   anchor;
		ref(Command)  command;
		ref(Footnote) footnote;
	};

	BodyArray *nodes;
};

rsdef(self *, New, ref(Type) type);
def(void, Destroy);

static inline void BodyArray_Destroy(BodyArray *nodes) {
	each(node, nodes) {
		Body_Destroy(*node);
	}
}

#undef self
