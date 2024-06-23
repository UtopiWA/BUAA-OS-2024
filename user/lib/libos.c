#include <env.h>
#include <lib.h>
#include <mmu.h>

void exit(void) {
	// After fs is ready (lab5), all our open files should be closed before dying.
#if !defined(LAB) || LAB >= 5
	close_all();
#endif

	syscall_env_destroy(0);
	user_panic("unreachable code");
}

const volatile struct Env *env;
extern int main(int, char **);

// challenge-shell
#define LIBENV 4

void libmain(int argc, char **argv) {
	// set env to point at our env structure in envs[].
	env = &envs[ENVX(syscall_getenvid())];

	// challenge-shell
	volatile struct Env *fa = &envs[ENVX(env->env_parent_id)];
	volatile struct Env *ee = env;
	int i = 0;
	int grand = fa->env_parent_id;
	
	int r = main(argc, argv);

	while (ee->env_parent_id) {
		ee = &envs[ENVX(ee->env_parent_id)]; 
		i++;
	}

	if (i != LIBENV) {
		ipc_send(grand, r, 0, 0);
	}

	// exit gracefully
	exit();
}
