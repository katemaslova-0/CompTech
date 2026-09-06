#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <assert.h>

size_t STRINGS_NUM = 100;

bool    OutputWords (int line, int num_of_words, char ** words_buffer, int WORD_TO_REVERSE_IDX);
void    OutputReverseWord(char *word_for_reverse);
char ** FillPtrBuff(int num_of_words, char *string_buffer);
int     SepOnWords(char *input_str);

typedef struct str_info
{
    char **words_buffer;
    size_t num_of_words;
} StrInfo;

void OutputTheEnteredLines(StrInfo *strs_buffer, size_t strs_num);
void FreeMemory(char *string_buffer, StrInfo *strs_buffer, size_t strs_num);

int main(int argc, char * argv[])
{
    if (argc != 2 || !isdigit(*argv[1]))
    {
        printf("Incorrect input\n");
        printf("Please restart the program by passing the index of the word for the reverse as the 2nd parameter.\n");
        return -1;
    }

    int WORD_TO_REVERSE_IDX = atoi(argv[1]);

    StrInfo *strs_buffer = (StrInfo *)calloc(STRINGS_NUM, sizeof(StrInfo));

    char *string_buffer = NULL;
    bool if_word_idx_correct = false;
    size_t string_size = 0;
    ssize_t chars_num = 0;

    size_t strs_num = 0;

    while ((chars_num = getline(&string_buffer, &string_size, stdin)) != -1 && strcmp("!\n", string_buffer))
    {
        string_buffer[strlen(string_buffer) - 1] = '\0';
        int num_of_words = SepOnWords(string_buffer);

        if (num_of_words <= WORD_TO_REVERSE_IDX)
        {
            printf("The number of words entered is %d, which is less than %d\n", num_of_words, WORD_TO_REVERSE_IDX);
            printf("Please try again by entering at least %d words\n", WORD_TO_REVERSE_IDX + 1);
            continue;
        }

        StrInfo str_info = {NULL, 0};
        str_info.words_buffer = FillPtrBuff(num_of_words, string_buffer);
        str_info.num_of_words = num_of_words;
        assert(str_info.words_buffer);

        strs_buffer[strs_num++] = str_info;
    }

    if (strs_num == 0)
    {
        printf("You haven't entered any lines\n");
        FreeMemory(string_buffer, strs_buffer, strs_num);
        return 0;
    }

    OutputTheEnteredLines(strs_buffer, strs_num);

    printf("After the reverse, we get the following lines of words:\n");
    for (int i = 0; i < strs_num; i++)
    {
        printf("[%d] : ", i);
        if (!(if_word_idx_correct = OutputWords(i + 1, strs_buffer[i].num_of_words, strs_buffer[i].words_buffer, WORD_TO_REVERSE_IDX)))
            return -1;

        putchar('\n');
    }

    FreeMemory(string_buffer, strs_buffer, strs_num);

    return 0;
}

void FreeMemory(char *string_buffer, StrInfo *strs_buffer, size_t strs_num)
{
    assert(strs_buffer);

    free(string_buffer);

    for (int i = 0; i < strs_num; i++)
    {
        for (int j = 0; j < strs_buffer[i].num_of_words; j++)
        {
            free(strs_buffer[i].words_buffer[j]);
        }

        free(strs_buffer[i].words_buffer);
    }

    free(strs_buffer);
}

void OutputTheEnteredLines(StrInfo *strs_buffer, size_t strs_num)
{
    assert(strs_buffer);

    printf("You have entered the following lines of words:\n");
    for (int i = 0; i < strs_num; i++)
    {
        printf("[%d] : ", i);
        for (int j = 0; j < strs_buffer[i].num_of_words; j++)
        {
            printf("%s ", strs_buffer[i].words_buffer[j]);
        }
        putchar('\n');
    }
    putchar('\n');
}

bool OutputWords(int line, int num_of_words, char ** words_buffer, int WORD_TO_REVERSE_IDX)
{
    assert(words_buffer);

    if (num_of_words <= WORD_TO_REVERSE_IDX)
    {
        printf("the number of words in line %d is %d, which is less than %d\n", line, num_of_words, WORD_TO_REVERSE_IDX);
        return false;
    }

    for (int i = num_of_words - 1; i >= 0; i--)
    {
        if (i == WORD_TO_REVERSE_IDX)
        {
            OutputReverseWord(words_buffer[i]);
            continue;
        }

        printf("%s ", words_buffer[i]);
    }

    return true;
}

void OutputReverseWord(char *word_for_reverse)
{
    assert(word_for_reverse);

    size_t word_len = strlen(word_for_reverse);
    for (int i = word_len - 1; i >= 0; i--)
    {
        putchar(word_for_reverse[i]);
    }

    putchar(' ');
}

char ** FillPtrBuff(int num_of_words, char *string_buffer)
{
    assert(string_buffer);
    char *string_buffer_new = string_buffer;

    char **words_buffer = (char **)calloc(num_of_words, sizeof(char *));

    for (int i = 0; i < num_of_words; i++)
    {
        string_buffer_new = strchr(string_buffer, '\0') + 1;
        while (*string_buffer_new == ' ') string_buffer_new++;
        words_buffer[i] = strdup(string_buffer);
        string_buffer = string_buffer_new;
    }

    return words_buffer;
}

int SepOnWords(char *input_str)
{
    assert(input_str);

    while (*input_str == ' ')
        input_str++;
    if (*input_str == '\0') return 0;

    int num_of_words = 0;
    char *space = NULL;

    while (space = strchr(input_str, ' '))
    {
        *space = '\0';
        if (*(space + 1) != '\0' && *(space + 1) != '\n')
            num_of_words++;
        input_str = ++space;
        while (*input_str == ' ') input_str++;
    }
    num_of_words++;

    return num_of_words;
}
