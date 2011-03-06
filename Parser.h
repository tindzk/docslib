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
	ProtString name;
	ProtString options;
	Typography_Node *node;
};

Array(ref(Node), ref(Nodes));

class {
	Typography tyo;
};

rsdef(self, New);
def(void, Destroy);
def(void, Parse, ProtString path);
def(Typography_Node *, GetRoot);
def(ProtString, GetMeta, ProtString name);
def(ProtStringArray *, GetMultiMeta, ProtString name);
def(Body, GetBody, Typography_Node *node, ProtString ignore);
def(ref(Nodes) *, GetNodes, Typography_Node *node);
def(ref(Nodes) *, GetNodesByName, Typography_Node *node, ProtString name);
def(ref(Node), GetNodeByName, ProtString name);

#undef self
