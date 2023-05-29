/**
 * @file limine-os.c
 * @brief Limine bindings for os, time, fileio modules.
 *
 */
#include <config.h>
#include <menu.h>
#include <lib/print.h>
#include <lib/config.h>
#include <lib/time.h>
#include <lib/readline.h>
#include <mm/pmm.h>
#include <kuroko/vm.h>
#include <kuroko/util.h>

extern int krk_exitRepl;
extern void krk_repl(void);

KRK_Function(uname) {
	KrkValue result = krk_dict_of(0, NULL, 0);
	krk_push(result);
	krk_attachNamedObject(AS_DICT(result), "sysname",  (KrkObj*)S("Limine"));
	krk_attachNamedObject(AS_DICT(result), "release",  (KrkObj*)S(LIMINE_VERSION));
	krk_attachNamedObject(AS_DICT(result), "nodename", (KrkObj*)S(""));

	krk_attachNamedObject(AS_DICT(result), "version",  (KrkObj*)
#if defined(BIOS)
		S("BIOS")
#elif defined(UEFI)
		S("UEFI")
#else
		S("")
#endif
	);

	krk_attachNamedObject(AS_DICT(result), "machine",  (KrkObj*)
#if defined(__x86_64__)
		S("x86-64")
#elif defined(__aarch64__)
		S("aarch64")
#elif defined(__i386__)
		S("i386")
#elif defined(__riscv64)
		S("riscv64")
#else
		S("unknown")
#endif
	);

	return krk_pop();
}

/**
 * def config_get_value(key: str, index: int = 0, config: str = None) -> str
 */
KRK_Function(config_get_value) {
	char * config = NULL;
	char * key = NULL;
	size_t index = 0;
	if (!krk_parseArgs("s|Nz", (const char*[]){"key", "index", "config"}, &key, &index, &config)) return NONE_VAL();
	char * value = config_get_value(config, index, key);

	if (!value) {
		return NONE_VAL();
	}

	return OBJECT_VAL(krk_copyString(value, strlen(value)));
}

/**
 * def boot(config: str) -> does not return
 */
KRK_Function(boot) {
	char * config = NULL;
	if (!krk_parseArgs("s", (const char*[]){"config"}, &config)) return NONE_VAL();

	boot(config);
}

/**
 * def getchar() -> int
 */
KRK_Function(getchar) {
	return INTEGER_VAL(getchar());
}

/**
 * def readline(prefix="",bufsize=1024) -> str
 */
KRK_Function(readline) {
	int size = 1024;
	char * prefix = "";
	if (!krk_parseArgs("|si", (const char*[]){"prefix","bufsize"}, &prefix, &size)) return NONE_VAL();

	char * buf = malloc(size);
	readline(prefix,buf,size);
	krk_push(OBJECT_VAL(krk_copyString(buf,strlen(buf))));
	free(buf);

	return krk_pop();
}

/**
 * Builtins
 */
KRK_Function(exit) {
	krk_exitRepl = 1;
	return NONE_VAL();
}

KRK_Function(clear) {
	print("\e[2J\e[H");
	return NONE_VAL();
}

void krk_module_init_os(void) {
	KrkInstance * module = krk_newInstance(vm.baseClasses->moduleClass);
	krk_attachNamedObject(&vm.modules, "os", (KrkObj*)module);
	krk_attachNamedObject(&module->fields, "__name__", (KrkObj*)S("os"));
	krk_attachNamedValue(&module->fields, "__file__", NONE_VAL());
	BIND_FUNC(module,uname);

	/* Let's also make a Limine-specific module */
	KrkInstance * limine = krk_newInstance(vm.baseClasses->moduleClass);
	krk_attachNamedObject(&vm.modules, "limine", (KrkObj*)limine);
	krk_attachNamedObject(&limine->fields, "__name__", (KrkObj*)S("limine"));
	krk_attachNamedValue(&limine->fields, "__file__", NONE_VAL());
	BIND_FUNC(limine,config_get_value);
	BIND_FUNC(limine,boot);
	BIND_FUNC(limine,getchar);
	BIND_FUNC(limine,readline);

	/* And some builtins */

	BIND_FUNC(vm.builtins,exit);
	BIND_FUNC(vm.builtins,clear);
}

KRK_Function(time) {
	/* We don't have floats, so just return an int; 48 bits should be enough */
	return INTEGER_VAL(time());
}

void krk_module_init_time(void) {
	KrkInstance * module = krk_newInstance(vm.baseClasses->moduleClass);
	krk_attachNamedObject(&vm.modules, "time", (KrkObj*)module);
	krk_attachNamedObject(&module->fields, "__name__", (KrkObj*)S("time"));
	krk_attachNamedValue(&module->fields, "__file__", NONE_VAL());
	BIND_FUNC(module,time);
}

void krk_module_init_fileio(void) {
	/* Nope. Maybe later. */
}

/**
 * @brief Initialize the Kuroko VM state and prep the @c limine module.
 */
void limine_krk_init(void) {
	krk_initVM(0);
	krk_startModule("__main__");
	krk_interpret(
		"if True:\n"
		"    import limine\n"
		"    def config(modules=[],**kwargs):\n"
		"        let args = [f'{k.upper()}={v}' for k,v in kwargs.items()]\n"
		"        for module in modules:\n"
		"            args.append(f'MODULE_PATH={module}')\n"
		"        return '\\n'.join(args)\n"
		"    limine.config = config\n",
		"<stdin>");
}

/**
 * @brief Start a repl session.
 */
void limine_krk_enter_repl(void) {
	/* Print version number. */
	krk_interpret(
		"if True:\n"
		"    import kuroko\n"
		"    print(f'Kuroko {kuroko.version} ({kuroko.builddate}) with {kuroko.buildenv}')\n"
		"    print('Type `help` for guidance, `exit()` to quit.')\n",
		"<stdin>");

	/* Run the repl */
	krk_repl();
}

/**
 * @brief Cleanup VM resources.
 */
void limine_krk_cleanup(void) {
	/* Free the VM resources; we never actually release the heap, so this doesn't really do anything,
	 * but the heap is shared through multiple Kuroko sessions, so freeing it is still useful. */
	krk_freeVM();
}

