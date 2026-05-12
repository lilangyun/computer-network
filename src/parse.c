#include "parse.h"

/**
* Given a char buffer returns the parsed request headers
*/
Request * parse(char *buffer, int size, int socketFd) {  // socketFD not used?
  //Differant states in the state machine
	enum {
		STATE_START = 0, STATE_CR, STATE_CRLF, STATE_CRLFCR, STATE_CRLFCRLF
	};

	int i = 0, state;
	size_t offset = 0;
	char ch;
	char buf[8192];
	memset(buf, 0, 8192);
	int num = 0, header_num = 0;

	state = STATE_START;
	while (state != STATE_CRLFCRLF) {
		char expected = 0;

		if (i == size)
			break;

		ch = buffer[i++];
		buf[offset++] = ch;

		switch (state) {
		case STATE_START:
		case STATE_CRLF:
			expected = '\r';
			break;
		case STATE_CR:
		case STATE_CRLFCR:
		    num++;
			expected = '\n';
			break;
		default:
			state = STATE_START;
			continue;
		}

		if (ch == expected)
			state++;
		else
			state = STATE_START;
	}

	// fprintf(stderr, "Checkpoint [2]\n");

    // Valid End State
	if (state == STATE_CRLFCRLF) {
		Request *request = (Request *) malloc(sizeof(Request));
        request->header_count=0;
        // TODO You will need to handle resizing this in parser.y
		if(num <= 2){
			header_num = 0;
		}
		else{
			header_num = num - 2;
		}
		request->headers = (Request_header *) malloc(sizeof(Request_header) * header_num);
		set_parsing_options(buf, i, request);

		// fprintf(stderr, "Checkpoint [3]\n");
		if (yyparse() == SUCCESS) {
			return request;
		}
	}
    // TODO Handle Malformed Requests
	// fprintf(stderr, "Checkpoint [3]\n"); 

	// destroy the bad buffer
	yylex_destroy();
    printf("Parsing Failed\n");
	return NULL;
}

