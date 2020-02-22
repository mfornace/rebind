#pragma once

#ifdef __cplusplus
extern "C" {
#endif

typedef struct rebind_variable rebind_variable;

void rebind_destruct(rebind_variable *x);

rebind_variable * rebind_variable_new();

rebind_variable * rebind_variable_copy(rebind_variable *v);


int rebind_add();


#ifdef __cplusplus
}
#endif