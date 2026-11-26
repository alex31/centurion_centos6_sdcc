#include <stdint.h>
#include <stdlib.h>

static uint32_t random_state = 1;

void srand(unsigned int seed)
{
    random_state = seed;
}

int rand(void)
{
    random_state = random_state * 1103515245UL + 12345UL;
    return (int)((random_state >> 16) & RAND_MAX);
}

static void swap(unsigned char *left, unsigned char *right, size_t size)
{
    while (size--) {
        unsigned char byte = *left;
        *left++ = *right;
        *right++ = byte;
    }
}

static void sift(unsigned char *base, size_t root, size_t count, size_t size,
                 int (*compare)(const void *, const void *))
{
    while (root < count / 2) {
        size_t child = 2 * root + 1;
        unsigned char *parent = base + root * size;
        unsigned char *larger = base + child * size;
        if (child + 1 < count && compare(larger, larger + size) < 0) {
            ++child;
            larger += size;
        }
        if (compare(parent, larger) >= 0)
            break;
        swap(parent, larger, size);
        root = child;
    }
}

void qsort(void *base, size_t count, size_t size,
           int (*compare)(const void *, const void *))
{
    unsigned char *array = base;
    size_t root = count / 2;
    if (!size)
        return;
    /* Heap sort keeps the C stack bounded for every input order. */
    while (root)
        sift(array, --root, count, size, compare);
    while (count > 1) {
        swap(array, array + --count * size, size);
        sift(array, 0, count, size, compare);
    }
}
