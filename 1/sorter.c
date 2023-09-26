#include <stdio.h>

void merge_sort(int * numbers, int count) {

}

int main(void)
{
    const char * filename = "test1.txt";

    FILE* file_ptr = fopen(filename, "r");
    if (file_ptr == NULL)
        return 1;

    int numbers[] = malloc(sizeof(int));
    int num;
    while (fscanf(file_ptr, "%d", &num) == 1) {
        printf("%d\n", num);
    }
    fclose(file_ptr);

    return 0;
}