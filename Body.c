#import "Body.h"

#define self Body

rsdef(self *, New, ref(Type) type) {
	self *ptr = Memory_New(sizeof(self));

	ptr->type  = type;
	ptr->nodes = BodyArray_New(ref(DefaultLength));

	return ptr;
}

def(void, Destroy) {
	if (this->type == ref(Type_Url)) {
		String_Destroy(&this->url.url);
	} else if (this->type == ref(Type_Text)) {
		String_Destroy(&this->text.value);
	} else if (this->type == ref(Type_Image)) {
		String_Destroy(&this->image.path);
	} else if (this->type == ref(Type_Command)) {
		String_Destroy(&this->command.value);
	} else if (this->type == ref(Type_Code)) {
		String_Destroy(&this->code.value);
	} else if (this->type == ref(Type_Mail)) {
		String_Destroy(&this->mail.addr);
	} else if (this->type == ref(Type_Anchor)) {
		String_Destroy(&this->anchor.name);
	} else if (this->type == ref(Type_Jump)) {
		String_Destroy(&this->jump.anchor);
	}

	if (this->nodes != NULL) {
		BodyArray_Destroy(this->nodes);
		BodyArray_Free(this->nodes);
	}

	this->type  = ref(Type_Empty);
	this->nodes = NULL;
}
