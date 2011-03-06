#import "TextDocument.h"

#define self TextDocument

def(void, Init, size_t lineLength) {
	this->doc  = String_New(1024);
	this->line = $("");

	this->fixed  = 0;
	this->indent = 0;
	this->offset = 0;

	this->lineLength = lineLength;
}

def(void, Destroy) {
	String_Destroy(&this->doc);
}

def(void, SetFixedIndent, size_t fixed) {
	this->fixed = fixed;
}

def(bool, HasTrailingLine) {
	return
		(this->doc.len > 0) &&
		(this->doc.buf[this->doc.len - 1] == '\n');
}

def(void, AddLine) {
	String_Append(&this->doc, '\n');

	this->line.buf = this->doc.buf + this->doc.len;
	this->line.len = 0;

	this->offset = this->doc.len;

	repeat (this->fixed) {
		String_Append(&this->doc, ' ');
		this->line.len++;
	}

	repeat (this->indent) {
		String_Append(&this->doc, $("  "));
		this->line.len += 2;
	}
}

def(void, Indent) {
	this->indent++;
}

def(void, Unindent) {
	if (this->indent == 0) {
		throw(InvalidDepth);
	}

	this->indent--;
}

/* Word-aware String_Slice. */
static def(ProtString, _Slice, ProtString s, size_t pos) {
	size_t orig = s.len;

	reverse(i, pos) {
		s.len = i;

		if (s.buf[i] == ' ' ||
			s.buf[i] == '/' ||
			s.buf[i] == '.' ||
			s.buf[i] == '?' ||
			s.buf[i] == ':' ||
			s.buf[i] == ';' ||
			s.buf[i] == '#' ||
			s.buf[i] == '(' ||
			s.buf[i] == ')' ||
			s.buf[i] == '|' ||
			s.buf[i] == '[' ||
			s.buf[i] == ']')
		{
			break;
		}
	}

	if (s.len == 0) {
		s.len = orig;
	}

	return s;
}

static def(void, _Add, ProtString s) {
	if (this->line.len + s.len > this->lineLength) {
		ssize_t pos = this->lineLength - this->line.len;

		ProtString add = call(_Slice, s, pos);
		String_Append(&this->doc, add);

		if (s.len > add.len) {
			ProtString next = String_Slice(s, add.len + 1);

			call(AddLine);
			call(_Add, next);
		}

		return;
	}

	/* Don't trim in the middle of a line. */
	if (this->indent == 0 && this->line.len == 0) {
		s = String_Trim(s, String_TrimLeft);
	}

	String_Append(&this->doc, s);

	this->line.buf = this->doc.buf + this->offset;
	this->line.len += s.len;
}

/* Makes sure that AddLine() is called for each line break. */
def(void, Add, ProtString s) {
	ProtStringArray *lines = String_Split(s, '\n');

	foreach (line, lines) {
		call(_Add, *line);

		if (lines->len > 1) {
			call(AddLine);
		}
	}

	ProtStringArray_Free(lines);
}
