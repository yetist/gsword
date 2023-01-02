#include <stdio.h>
#include <stdlib.h>
#include <gsword/gsword.h>

int main(int argc, char **argv)
{
	GswManager* manager;
	GswModule *module;
	char key[50];
	if (argc != 2) {
		fprintf(stderr, "usage: %s <modName>\n", *argv);
		exit(-1);
	}
	manager = gsw_manager_new();
	module = gsw_manager_get_module_by_name (manager, argv[1]);

	if (module == NULL) {
		fprintf(stderr, "%s: couldn't find module: %s\n", *argv, argv[1]);
		exit(-1);
	}

	printf("\nPress [CTRL-C] to end\n\n");
	while (TRUE) {
		printf("\nModule text:\n");
		gsw_module_set_key_text(module, "1jn 1:9");
		printf("[ %s ]\n", gsw_module_get_key_text(module));
		printf("%s", gsw_module_render_text(module));
		printf("\n\nEnter new cipher key: ");
		if (scanf("%[^\n]", key)){};
		printf("\nSetting key to: %s",  key);
		gsw_manager_set_cipherkey(manager, argv[1], key);
	}

	return 0;
}
