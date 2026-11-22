/*
 * matrix_add.c - 두 행렬의 크기를 입력받아 동적 메모리를 할당하고 더하는 프로그램
 *
 * 입력 순서: A 의 행 열 / B 의 행 열 / A 의 원소(행 단위) / B 의 원소(행 단위)
 * 행렬은 1차원 int 배열에 행 우선(row-major)으로 저장하며,
 * 크기를 입력받은 뒤 malloc 으로 필요한 만큼만 할당하고 끝나면 free 한다.
 *
 * 컴파일: gcc -Wall -o matrix_add matrix_add.c
 */
#include <stdio.h>
#include <stdlib.h>
#include <limits.h>

/* rows x cols 개의 int 를 동적 할당하고 사용자로부터 원소를 입력받는다. 실패 시 NULL */
static int *read_matrix(const char *name, int rows, int cols)
{
    int len = rows * cols;
    int *m = (int *)malloc((size_t)len * sizeof(int));
    if (m == NULL) {
        fprintf(stderr, "오류: 메모리를 할당하지 못했습니다\n");
        return NULL;
    }
    printf("행렬 %s 의 원소 %d개를 행 단위로 입력하세요:\n", name, len);
    for (int i = 0; i < len; i++) {
        if (scanf("%d", &m[i]) != 1) {
            fprintf(stderr, "오류: 올바른 정수 입력이 아닙니다\n");
            free(m);
            return NULL;
        }
    }
    return m;
}

static void print_matrix(const char *name, const int *m, int rows, int cols)
{
    printf("%s:\n", name);
    for (int i = 0; i < rows; i++) {
        for (int j = 0; j < cols; j++)
            printf("%6d", m[i * cols + j]);
        printf("\n");
    }
}

int main(void)
{
    int ar, ac, br, bc;
    int *a = NULL, *b = NULL, *sum = NULL;
    int ret = 1;

    printf("행렬 A 의 행 수와 열 수: ");
    if (scanf("%d %d", &ar, &ac) != 2 || ar <= 0 || ac <= 0) {
        fprintf(stderr, "오류: A 의 크기가 올바르지 않습니다\n");
        return 1;
    }
    printf("행렬 B 의 행 수와 열 수: ");
    if (scanf("%d %d", &br, &bc) != 2 || br <= 0 || bc <= 0) {
        fprintf(stderr, "오류: B 의 크기가 올바르지 않습니다\n");
        return 1;
    }
    if (ar != br || ac != bc) {
        fprintf(stderr, "오류: 크기가 다른 행렬은 더할 수 없습니다 (A = %dx%d, B = %dx%d)\n",
                ar, ac, br, bc);
        return 1;
    }
    if (ar > INT_MAX / ac) {
        fprintf(stderr, "오류: 행렬 크기가 너무 큽니다\n");
        return 1;
    }

    a = read_matrix("A", ar, ac);
    if (a == NULL) goto done;
    b = read_matrix("B", br, bc);
    if (b == NULL) goto done;

    sum = (int *)malloc((size_t)ar * ac * sizeof(int));
    if (sum == NULL) {
        fprintf(stderr, "오류: 메모리를 할당하지 못했습니다\n");
        goto done;
    }
    for (int i = 0; i < ar * ac; i++) {
        if ((b[i] > 0 && a[i] > INT_MAX - b[i]) || (b[i] < 0 && a[i] < INT_MIN - b[i])) {
            fprintf(stderr, "오류: %d + %d 에서 오버플로 발생\n", a[i], b[i]);
            goto done;
        }
        sum[i] = a[i] + b[i];
    }

    printf("\n");
    print_matrix("A", a, ar, ac);
    print_matrix("B", b, br, bc);
    print_matrix("A + B", sum, ar, ac);
    ret = 0;

done:
    free(a);        /* free(NULL) 은 안전하다 */
    free(b);
    free(sum);
    return ret;
}
