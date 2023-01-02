#include <stdio.h>
#include "gsword.h"

int main(int argc, char **argv)
{
	GswManager* manager;
	GList *list, *l;
	GswModule* module;

	manager = gsw_manager_new();
	list = gsw_manager_get_modules (manager);

	for (l = list; l != NULL; l=l->next) {
		module = (GswModule*) l->data;
		printf("%s|%s\n", gsw_module_get_mod_name(module), gsw_module_get_mod_type_str(module));
	}
	l = list;
	module = (GswModule*) l->data;
	printf("\n%s:\n%s (%s)\n", gsw_module_get_key_text(module), gsw_module_render_text(module), gsw_module_get_mod_name(module));

	l = l->next;
	module = (GswModule*) l->data;
	printf("\n%s:\n%s (%s)\n", gsw_module_get_key_text(module), gsw_module_render_text(module), gsw_module_get_mod_name(module));

	return 0;
}
