#include "glrender.h"

int main()
{
	int error;
	
	error = window_init();
	if (error != 0) {
		return 1;
	}

	loop();

	return 0;
}
