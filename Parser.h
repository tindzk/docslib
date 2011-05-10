#import <File.h>
#import <String.h>
#import <Logger.h>
#import <Integer.h>
#import <Typography.h>
#import <FileStream.h>
#import <BufferedStream.h>

#import "Body.h"

#define self Parser

record(ref(Node)) {
	RdString name;
	RdString options;
	Typography_Node *node;
};

Callback(ref(OnNode), void, ref(Node) *node);

record(ref(Handler)) {
	RdString    name;
	ref(OnNode) onNode;
};

class {
	Typography tyo;
	Logger     *logger;
	BodyArray  *footnotes;
	bool       autoDetectParagraphs;
};

rsdef(self, New);
def(void, Destroy);
def(void, SetAutoDetectParagraphs, bool value);
def(BodyArray *, GetFootnotes);
def(Typography_Node *, GetRoot);
def(void, Parse, RdString path);
def(RdString, GetMeta, RdString name);
def(RdStringArray *, GetMultiMeta, RdString name);
def(void, ProcessNodes, Typography_Node *node, ref(Handler) *handlers);
def(Body, ProcessBody, Typography_Node *node, ref(Handler) *handlers);
def(ref(Node), GetNodeByName, RdString name);

#undef self
