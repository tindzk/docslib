#import <File.h>
#import <String.h>
#import <Logger.h>
#import <Integer.h>
#import <Typography.h>
#import <FileStream.h>
#import <BufferedStream.h>

#import "Body.h"

#undef self
#define self Parser

record(ref(Node)) {
	String name;
	String options;
	Typography_Node *node;
};

Array_Define(ref(Node), ref(Nodes));

class(self) {
	Typography tyo;
};

ExtendClass(self);

def(void, Init);
def(void, Destroy);
def(void, Parse, String path);
def(Typography_Node *, GetRoot);
def(String, GetMeta, String name);
def(Body, GetBody, Typography_Node *node, String ignore);
def(ref(Nodes) *, GetNodes, Typography_Node *node);
def(ref(Node), GetNodeByName, String name);
