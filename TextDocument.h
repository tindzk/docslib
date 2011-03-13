#import <String.h>

#define self TextDocument

// @exc InvalidDepth

class {
	String doc;
	RdString line; /* Points to the current line in `doc'. */
	size_t offset;
	size_t fixed;
	size_t indent;
	size_t lineLength;
};

def(void, Init, size_t lineLength);
def(void, Destroy);
def(void, SetFixedIndent, size_t fixed);
def(bool, HasTrailingLine);
def(void, AddLine);
def(void, Indent);
def(void, Unindent);
def(void, Add, RdString s);

#undef self
