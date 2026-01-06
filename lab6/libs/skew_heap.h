#ifndef __LIBS_SKEW_HEAP_H__
#define __LIBS_SKEW_HEAP_H__

struct skew_heap_entry {
     struct skew_heap_entry *parent, *left, *right; // 父节点、左子节点、右子节点
};

typedef struct skew_heap_entry skew_heap_entry_t;

typedef int(*compare_f)(void *a, void *b); // 比较函数指针类型

static inline void skew_heap_init(skew_heap_entry_t *a) __attribute__((always_inline));
static inline skew_heap_entry_t *skew_heap_merge(
     skew_heap_entry_t *a, skew_heap_entry_t *b,
     compare_f comp);
static inline skew_heap_entry_t *skew_heap_insert(
     skew_heap_entry_t *a, skew_heap_entry_t *b,
     compare_f comp) __attribute__((always_inline));
static inline skew_heap_entry_t *skew_heap_remove(
     skew_heap_entry_t *a, skew_heap_entry_t *b,
     compare_f comp) __attribute__((always_inline));

static inline void
skew_heap_init(skew_heap_entry_t *a)
{
     a->left = a->right = a->parent = NULL; // 初始化节点，清空指针
}

static inline skew_heap_entry_t *
skew_heap_merge(skew_heap_entry_t *a, skew_heap_entry_t *b,
                compare_f comp)
{
     if (a == NULL) return b; // 如果a为空，返回b
     else if (b == NULL) return a; // 如果b为空，返回a
     
     skew_heap_entry_t *l, *r;
     if (comp(a, b) == -1) // 如果a < b（根据比较规则）
     {
          r = a->left; // 保存a的左子树
          l = skew_heap_merge(a->right, b, comp); // 递归合并a的右子树和b
          
          a->left = l; // 将合并结果作为a的左子树（交换左右子树的关键步骤）
          a->right = r; // 将原左子树作为a的右子树
          if (l) l->parent = a; // 更新父指针

          return a;
     }
     else
     {
          r = b->left; // 保存b的左子树
          l = skew_heap_merge(a, b->right, comp); // 递归合并a和b的右子树
          
          b->left = l; // 将合并结果作为b的左子树
          b->right = r; // 将原左子树作为b的右子树
          if (l) l->parent = b; // 更新父指针

          return b;
     }
}

static inline skew_heap_entry_t *
skew_heap_insert(skew_heap_entry_t *a, skew_heap_entry_t *b,
                 compare_f comp)
{
     skew_heap_init(b); // 初始化新节点b
     return skew_heap_merge(a, b, comp); // 将b合并到堆a中
}

static inline skew_heap_entry_t *
skew_heap_remove(skew_heap_entry_t *a, skew_heap_entry_t *b,
                 compare_f comp)
{
     skew_heap_entry_t *p   = b->parent; // 获取b的父节点
     skew_heap_entry_t *rep = skew_heap_merge(b->left, b->right, comp); // 合并b的左右子树
     if (rep) rep->parent = p; // 更新父指针
     
     if (p)
     {
          if (p->left == b)
               p->left = rep; // 如果b是父节点的左子节点，更新为合并后的子树
          else p->right = rep; // 否则更新右子节点
          return a; // 返回堆的根节点a
     }
     else return rep; // 如果b是根节点，返回新的根节点rep
}

#endif    /* !__LIBS_SKEW_HEAP_H__ */
