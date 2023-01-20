#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

typedef struct stock
{
    char *symbol;
    double price;
    int quantity;
} Stock;

char *randomSymbolCreator(char *symbol);

double randomPriceGenerator();
double updatePrice(double price);
double calcNetWorth(Stock *stocks, double netWorth, double cash);

void initializeStockMarket(Stock *stocks);
void initializeApp();
void freeMemory(Stock *stocks, int size);
void listStocks(Stock *stocks);
void choosesPrinter();
void calculateTotalStocksValue(Stock *stocks);

int main()
{
    Stock stocks[20];
    double cash = 5000, netWorth;
    int quarter = 1, year = 2023, age, yearLeft = 20;
    char name[20];
    initializeApp();
    // Initializing the StockMarket
    srand(time(NULL));
    initializeStockMarket(stocks);
    printf("How old are you: ");
    scanf("%d", &age);
    printf("What's your name: ");
    scanf("%s", name);
    char *inputStock = (char *)malloc(5 * sizeof(char));
    while (1)
    {
        if (quarter >= 5)
        {
            quarter = 1;
            year += 1;
            age += 1;
            yearLeft -= 1;
        }
        usleep(250000);
        netWorth = calcNetWorth(stocks, netWorth, cash);
        if (yearLeft > 0)
        {
            printf("\nHello %s.We are in the Q%d of %d and you are %d. You have %d years left to reach 10 million dollars.\n", name, quarter, year, age, yearLeft);
        }
        else
        {
            if (netWorth >= 10000000)
            {
                printf("\nCong. You win \n");
                return 1;
            }
            else
            {
                printf("\nGame Over! Try again. Your final Net Worth is $%.2lf\n", netWorth);
                return 1;
            }
        }

    here:
        usleep(250000);
        printf("Net Worth: $%.2lf", netWorth);
        usleep(250000);
        printf("\t\tCASH: $%.2lf", cash);
        usleep(250000);
        calculateTotalStocksValue(stocks);
        printf("\n");
        choosesPrinter();
        int choice;
        scanf("%d", &choice);

        switch (choice)
        {
        case 1:
            listStocks(stocks);
            break;
        case 2:
            listStocks(stocks);
            printf("\n\nEnter the symbol of the stock: ");
            scanf("%s", inputStock);
            inputStock[5] = '\0';
            for (int i = 0; i < 20; i++)
            {
                if (strcmp(inputStock, stocks[i].symbol) == 0)
                {
                    int num = 0;
                    printf("\tHow many stock do you want to buy: ");
                    scanf("%d", &num);
                    if (cash >= num * stocks[i].price)
                    {
                        stocks[i].quantity += num;
                        cash -= num * stocks[i].price;
                    }
                    else
                        printf("\nYou don't have enough money for this. Sell some stocks first!\n");
                }
            }
            break;
        case 3:
            listStocks(stocks);
            printf("\n\nEnter the symbol of the stock: ");
            scanf("%s", inputStock);
            inputStock[5] = '\0';
            for (int i = 0; i < 20; i++)
            {
                if (strcmp(inputStock, stocks[i].symbol) == 0)
                {
                    int num = 0;
                    printf("\tHow many stock do you want to sell: ");
                    scanf("%d", &num);
                    stocks[i].quantity -= num;
                    cash += num * stocks[i].price;
                }
            }

            break;
        default:
            printf("Invalid choice. Please try again.\n");
            break;
        }

        int check;
        printf("Pass to next quarter?: (y=1,n=any)");
        scanf("%d", &check);
        if (check == 1)
        {
            for (int i = 0; i < 20; i++)
            {
                stocks[i].price = updatePrice(stocks[i].price);
            }
            printf("\n--------------------------------\n");
            quarter += 1;
            continue;
        }
        else
            goto here;
    }
    free(inputStock);
    freeMemory(stocks, 20);
    return 0;
}

void listStocks(Stock *stocks)
{
    for (int i = 0; i < 20; i++)
    {
        printf("\n%s | %.2lf | %d", stocks[i].symbol, stocks[i].price, stocks[i].quantity);
    }
    printf("\n");
}

void choosesPrinter()
{
    printf("******************");
    printf("\nWhat would you like to do?\n");
    printf("1. View stocks\n");
    printf("2. Buy stocks\n");
    printf("3. Sell stocks\n");
    printf("******************\n\n");
}

void calculateTotalStocksValue(Stock *stocks)
{
    double totalValue = 0;
    for (int i = 0; i < 20; i++)
    {
        for (int j = 0; j < stocks[i].quantity; j++)
        {
            totalValue += stocks[i].price;
        }
    }
    printf("\t\tTotal value of stocks: $%.2lf\n", totalValue);
}

void initializeApp()
{
    printf("\n\nWelcome to the Stock Market Game!\n");
    sleep(1);
    printf("In this game, you will start with $5,000 and have 20 years to reach a net worth of $10,000,000.\n");
    sleep(3);
    printf("You will be simulating a quarter at a time, or 3 months.\n");
    sleep(2);
    printf("Are you ready to begin? Starting in 3...\n");
    sleep(1);
    printf("2...\n");
    sleep(1);
    printf("1...\n");
    sleep(1);
    printf("Let's go!\n");
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

double calcNetWorth(Stock *stocks, double netWorth, double cash)
{
    netWorth = cash;
    for (int i = 0; i < 20; i++)
    {
        if (stocks[i].quantity != 0)
        {
            netWorth += stocks[i].price * stocks[i].quantity;
        }
    }
    return netWorth;
}