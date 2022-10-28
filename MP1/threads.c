#include "kernel/types.h"
#include "user/setjmp.h"
#include "user/threads.h"
#include "user/user.h"
#define NULL 0

static struct thread* current_thread = NULL;
static struct thread* root_thread = NULL;
static int id = 1;
static jmp_buf env_st;
//static jmp_buf env_tmp;
// TODO: necessary declares, if any

void preorder(struct thread *t){
    printf("%d", t->ID);
    if(t->left != NULL)
        preorder(t->left);
    if(t->right != NULL)
        preorder(t->right);
}

void traverse(struct thread *t){
    preorder(t);
    printf("\n");
}

struct thread *thread_create(void (*f)(void *), void *arg){
    struct thread *t = (struct thread*) malloc(sizeof(struct thread));
    //unsigned long stack_p = 0;
    unsigned long new_stack_p;
    unsigned long new_stack;
    new_stack = (unsigned long) malloc(sizeof(unsigned long)*0x100);
    new_stack_p = new_stack +0x100*8-0x2*8;
    t->fp = f;
    t->arg = arg;
    t->ID  = id;
    t->buf_set = 0;
    t->stack = (void*) new_stack;
    t->stack_p = (void*) new_stack_p;
    t->left = NULL;
    t->right = NULL;
    t->parent = NULL;
    id++;
    return t;
}

void thread_add_runqueue(struct thread *t){
    if(root_thread == NULL)
        root_thread = t;
    else if(current_thread == NULL){
        free(t->stack);
        free(t);
    }
    else{
        if(current_thread->left == NULL){
            current_thread->left = t;
            t->parent = current_thread;
        }
        else if(current_thread->right == NULL){
            current_thread->right = t;
            t->parent = current_thread;
        }
        else{
            free(t->stack);
            free(t);
        }
    }
    //traverse(root_thread);
}

void thread_yield(void){
    int ret = setjmp(current_thread->env);
    if(ret == 0){
        schedule();
        dispatch();
    }
}

void dispatch(void){
    if(current_thread->buf_set == 0){
        int ret = setjmp(current_thread->env);
        if(ret == 0){
            current_thread->env->sp = (unsigned long)current_thread->stack_p;
            longjmp(current_thread->env, 1);
        }
        current_thread->buf_set = 1;
        current_thread->fp(current_thread->arg);
    }
    else
        longjmp(current_thread->env, 1);
    thread_exit();
}

void schedule(void){
    if(current_thread->left != NULL) current_thread = current_thread->left;
    else if(current_thread->right != NULL) current_thread = current_thread->right;
    // find the closest root which has a right child or is the root
    else{
        while(1){
            if(current_thread == root_thread) break;
            if(current_thread->parent->right == current_thread){
                current_thread = current_thread->parent;
                continue;
            }
            if(current_thread->parent->right == NULL){
                current_thread = current_thread->parent;
                continue;
            }
            current_thread = current_thread->parent->right;
            break;
        }
    }
}

void thread_exit(void){
    // No more thread to execute
    if(current_thread == root_thread && current_thread->left == NULL && current_thread->right == NULL){
        free(current_thread->stack);
        free(current_thread);
        // return to main
        longjmp(env_st, 1);
    }
    else{
        // find the "last" preorder node
        struct thread *last = current_thread;
        while(1){
            if(last->right != NULL){
                last = last->right;
                continue;
            }
            if(last->left != NULL){
                last = last->left;
                continue;
            }
            break;
        }
        //leaf
        if(last == current_thread){
            schedule();
            
            if(last->parent->left == last)
                last->parent->left = NULL;
            else
                last->parent->right = NULL;        
                 
            free(last->stack);
            free(last);
            
            dispatch();
        }
        // need to be replaced
        else{
            if(last->parent->left == last)
                last->parent->left = NULL;
            else
                last->parent->right = NULL;
            
            last->parent = current_thread->parent;
            if(current_thread->left != NULL){
                last->left = current_thread->left;
                last->left->parent = last;
            }
            if(current_thread->right != NULL){
                last->right = current_thread->right;
                last->right->parent = last;
            }
            
            if(current_thread != root_thread){            
                if(current_thread->parent->left == current_thread)
                    current_thread->parent->left = last;
                else
                    current_thread->parent->right = last;
            }
            else
                root_thread = last;
            
            //traverse(root_thread);
            
            free(current_thread->stack);
            free(current_thread);
            
            current_thread = last;
            schedule();
            dispatch();
        }
    }
}

void thread_start_threading(void){
    current_thread = root_thread;
    int ret = setjmp(env_st);
    if(ret==0) dispatch();
}