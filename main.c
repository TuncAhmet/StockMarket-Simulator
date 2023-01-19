#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

typedef struct stock
{
    char *symbol;
    double price;
    int quantity;
} Stock;

void initializeStockMarket(Stock *stocks);
char *randomSymbolCreator(char *symbol);
double randomPriceGenerator();
void freeMemory(Stock *stocks, int size);
double updatePrice(double price);

int main()
{
    // Initializing the StockMarket
    srand(time(NULL));
    Stock stocks[20];
    initializeStockMarket(stocks);
    double cash = 5000;
    while (1)
    {
        double netWorth = cash;
        for (int i = 0; i < 20; i++)
        {
            if (stocks[i].quantity != 0)
            {
                netWorth += stocks[i].price * stocks[i].quantity;
            }
        }
        printf("Net Worth: %.2lf", netWorth);
        printf("CASH: $%.2lf\n", cash);
        printf("\n");
        for (int i = 0; i < 20; i++)
        {
            printf("\n%s | %lf | %d", stocks[i].symbol, stocks[i].price, stocks[i].quantity);
        }
        printf("\n");
        char *inputStock = (char *)malloc(5 * sizeof(char));
        printf("Enter the symbol of the stock: ");
        scanf("%s", inputStock);
        inputStock[5] = '\0';
        for (int i = 0; i < 20; i++)
        {
            if (strcmp(inputStock, stocks[i].symbol) == 0)
            {
                int num = 0;
                printf("How many stock do you want to buy: ");
                scanf("%d", &num);
                stocks[i].quantity += num;
                cash -= num * stocks[i].price;
            }
        }

        int check;
        printf("Do you want to continue?(y=1,n=any)");
        scanf("%d", &check);
        if (check == 1)
        {
            for (int i = 0; i < 20; i++)
            {
                stocks[i].price = updatePrice(stocks[i].price);
            }
            printf("\n--------------------------------\n");
            continue;
        }
        else
        {
            printf("\n--------------------------------\n");
            break;
        }
    }

    freeMemory(stocks, 20);
    return 0;
}

void initializeStockMarket(Stock *stocks)
{
    for (int i = 0; i < 20; i++)
    {
        stocks[i].symbol = (char *)malloc(5 * sizeof(char));
        randomSymbolCreator(stocks[i].symbol);
        stocks[i].symbol[4] = '\0';
        stocks[i].price = randomPriceGenerator();
        stocks[i].quantity = 0;
    }
}

char *randomSymbolCreator(char *symbol)
{
    for (int i = 0; i < 4; i++)
    {
        char randomletter = "ABCDEFGHIJKLMNOPQRSTUVWXYZ"[rand() % 26];
        symbol[i] = randomletter;
    }
}

double randomPriceGenerator()
{
    return (double)rand() / RAND_MAX * 100;
}

void freeMemory(Stock *stocks, int size)
{
    for (int i = 0; i < size; i++)
    {
        free(stocks[i].symbol);
        stocks[i].symbol = NULL;
    }
}

double updatePrice(double price)
{
    double randNum = (double)rand() / RAND_MAX; // 0 ile 1 arasında rastgele sayı
    randNum = randNum * 1.5 - 0.5;              // -0.5 ile 1 arasında sayı elde etmek için
    return (double)price * (randNum + 1);
}
