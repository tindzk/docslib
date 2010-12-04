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
	String name;
	String options;
	Typography_Node *node;
};

Array(ref(Node), ref(Nodes));

class {
	Typography tyo;
};

def(void, Init);
def(void, Destroy);
def(void, Parse, String path);
def(Typography_Node *, GetRoot);
def(String, GetMeta, String name);
def(StringArray *, GetMultiMeta, String name);
def(Body, GetBody, Typography_Node *node, String ignore);
def(ref(Nodes) *, GetNodes, Typography_Node *node);
def(ref(Nodes) *, GetNodesByName, Typography_Node *node, String name);
def(ref(Node), GetNodeByName, String name);

#undef self
