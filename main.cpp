#include "GLRender.h"

int main()
{
	GLRender app;

	int error;
	
	error = app.window_init();
	if (error != 0) {
		return 1;
	}

	app.loop();

	return 0;
}
