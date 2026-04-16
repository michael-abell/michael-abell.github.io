#define NOMINMAX
#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <curl/curl.h>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <limits>
#include <nlohmann/json.hpp>
#include <regex>
#include <sstream>
#include <string>
#include <vector> 
#include "ItemTracking.h"
#include "sqlite3.h"
using namespace std;
using json = nlohmann::json;

// Constructor for the ItemTracking class
ItemTracking::ItemTracking() {
    
}

// User interface for the program, with a menu to select different options using a switch statement.
void ItemTracking::MainMenu() {
	int choice = 0;
    
    cout << "       *** Main Menu ***" << endl;                                                                         
    cout << endl;
    Analytics();
    cout << endl;
    cout << "Option 1: Add the item and quantity sold" << endl;
	cout << "---------------------------------------------" << endl;
    cout << "Option 2: Search a specific item and retreive the quantity sold today." << endl;
    cout << "Option 3: Displays a list of each item and its' quanity sold today." << endl;                              
    cout << "Option 4: Displays a list of each item and displays a graph of how may items were sold today." << endl;    
    cout << "Option 5: Quit program" << endl;                                                                           
    cout << endl;                                                                                                       
    cout << "Please Enter 1 2 3 4 or 5" << endl;

    // Read a full line and parse as integer to avoid stream errors when user types non-numeric input
    {
        string line;
        while (true) {
            if (!getline(cin, line)) {
                cin.clear();
                continue;
            }
            
            if (line.empty()) {
                cout << "Please enter a valid choice (1-5): ";
                continue;
            }

            stringstream ss(line);
            if (!(ss >> choice)) {
                cout << "Invalid input. Please enter a number between 1 and 5: ";
                continue;
            }

            string rest;
            if (ss >> rest) {
                cout << "Invalid input. Please enter a single number (1-5): ";
                continue;
            }

            if (choice < 1 || choice > 5) {
                cout << "Choice out of range. Enter a number between 1 and 5: ";
                continue;
            }

            break;
        }
    }
        
    switch (choice)                                                 
    {
    case 1:
        system("cls");
        AddItem();
        system("cls");
        MainMenu();
        break;
    case 2:
        system("cls");
        FindItem();                                                
        cin.ignore();
        system("cls");                                              
        MainMenu();                                                 
        break;
    case 3:
        system("cls");
        ListItems();                                                
        cin.ignore();
        system("cls");
        MainMenu();
        break;
    case 4:
        system("cls");
        GraphItems();
        cout << endl << "Press any key to continmue" << endl;
        cin.get();
        system("cls");        
        MainMenu();
        break;
    case 5:
        system("cls");                                              
        cout << "Goodbye" << endl;                                  
        exit(0);                                                    

    }
}

// Receive user input of item and qty, scrape price and assign it all to the DB
int ItemTracking::AddItem() {
	// Variables for DB connection and statements
    sqlite3* db;
    sqlite3_stmt* stmt;
    int rc;

    string itemName;
    int quantity;
    double cost;
    double value;
    char choice = 'y';

    while (choice == 'y' || choice == 'Y') {

		// Ask for item name, don't allow empty input, and ignore any leftover input in the buffer
        cout << "Enter item name: ";
       getline(cin, itemName);

        while (itemName.empty() || !all_of(itemName.begin(), itemName.end(),
            [](unsigned char c) { return isalpha(c) || isspace(c); })) {
            cout << "Item name cannot be empty or a number. Try again: ";
            getline(cin, itemName);
        }

		// Ask for quantity sold, validate that it's a positive integer and handle any non-integer input
        cout << "Enter quantity sold: ";
        cin >> quantity;
        while (cin.fail() || quantity <= 0) {
            cin.clear();
            cin.ignore(numeric_limits<streamsize>::max(), '\n');
            cout << "Invalid quantity. Enter a positive number: ";
            cin >> quantity;
        }

		// Call the ScraperPrice function to get the cost of the item
        cost = ScrapePrice(itemName);

        if (cost == 0.0) {
            cout << "Could not fetch price, enter manually: ";
            cin >> cost;
        }

		// Simple variable formula to calculate the total value of the items sold
        value = cost * quantity;
        
		// Open the DB connection, handle any errors and ensure the DB is closed if an error occurs
        rc = sqlite3_open("Inventory.db", &db);
        if (rc) {
            cerr << "DB open error: " << sqlite3_errmsg(db) << endl;
            sqlite3_close(db);
            return rc;
        }

		// Prepare the SQL statement for inserting the new item, handle any errors and ensure the DB is closed if an error occurs
        const char* sql =
            "INSERT INTO Inventory (ITEM, QTY, COST, VALUE) "
            "VALUES (?, ?, ?, ?);";

        rc = sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr);
        if (rc != SQLITE_OK) {
            cerr << "Prepare failed: " << sqlite3_errmsg(db) << endl;
            sqlite3_close(db);
            return rc;
        }

        // Bind input and data to statement (https://sqlite.org/c3ref/bind_blob.html)
        sqlite3_bind_text(stmt, 1, itemName.c_str(), -1, SQLITE_STATIC);
        sqlite3_bind_int(stmt, 2, quantity);
        sqlite3_bind_double(stmt, 3, cost);
        sqlite3_bind_double(stmt, 4, value);

        // Execute statement https://sqlite.org/c3ref/step.html
        rc = sqlite3_step(stmt);
        if (rc != SQLITE_DONE) {
            cerr << "Insert failed: " << sqlite3_errmsg(db) << endl;
        }
        else {
            cout << "Item added successfully.\n";
        }

        // Cleanup
        sqlite3_finalize(stmt);
        sqlite3_close(db);

        // Loop: validate yes/no input
        {
            string yn;
            while (true) {
                cout << "Would you like to add another? (y/n): ";
                if (!(cin >> yn)) {
                    cin.clear();
                    cin.ignore(numeric_limits<streamsize>::max(), '\n');
                    choice = 'n';
                    break;
                }

                if (yn.empty()) continue;
                char c = static_cast<char>(tolower(static_cast<unsigned char>(yn[0])));
                if (c == 'y' || c == 'n') {
                    choice = c;
                    // consume the rest of the line if user typed extra
                    cin.ignore(numeric_limits<streamsize>::max(), '\n');
                    break;
                }

                cout << "Invalid input. Please enter 'y' or 'n'." << endl;
                // discard rest of line and retry
                cin.ignore(numeric_limits<streamsize>::max(), '\n');
            }
        }
    }

    // If user chose 'n', show a pause message before returning to menu
    if (choice == 'n') {
        cout << endl << "You're done, press any key" << endl;
        cin.clear();
    }

    return 0;
}

// Type in an item name and see how many have sold today
int ItemTracking::FindItem() {	                                    
    
    sqlite3* db = nullptr;
    sqlite3_stmt* stmt = nullptr;
    int rc;

    string itemName;

    // Input
    cout << "Enter item name to search: ";
    getline(cin, itemName);

    while (itemName.empty()) {
        cout << "Item name cannot be empty. Try again: ";
        getline(cin, itemName);
    }

    // Open DB
    rc = sqlite3_open("Inventory.db", &db);
    if (rc != SQLITE_OK) {
        cerr << "DB open error: " << sqlite3_errmsg(db) << endl;
        if (db) sqlite3_close(db);
        return rc;
    }

    // Prepare the SQL statement
    const char* sql =
        "SELECT COALESCE(SUM(QTY), 0) "
        "FROM Inventory "
        "WHERE LOWER(ITEM) = LOWER(?) "
        "AND DATE(DATETIME) = DATE('now','localtime');";

    rc = sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr);
    if (rc != SQLITE_OK) {
        cerr << "Prepare failed: " << sqlite3_errmsg(db) << endl;
        sqlite3_close(db);
        return rc;
    }

    // Bind statement
    sqlite3_bind_text(stmt, 1, itemName.c_str(), -1, SQLITE_STATIC);

    // Execute statement
    rc = sqlite3_step(stmt);
    if (rc == SQLITE_ROW) {
        int total = sqlite3_column_int(stmt, 0);

        cout << "Item: " << itemName << endl;
        cout << "Quantity sold today: " << total << endl;

        sqlite3_finalize(stmt);
        sqlite3_close(db);
        cout << "Press enter to return to the main screen..." << endl;
        return total;
    }

    cerr << "Query failed: " << sqlite3_errmsg(db) << endl;

    // Cleanup
    sqlite3_finalize(stmt);
    sqlite3_close(db);
	cout << "Press enter to return to the main screen..." << endl;
    return 0;

}

// Print Item name then number of how many times it occcured today
void ItemTracking::ListItems() {
    // Variables for DB connection and statements
    sqlite3* db = nullptr;
    sqlite3_stmt* stmt = nullptr;

	// Open DB 
    int rc = sqlite3_open("Inventory.db", &db);
    if (rc != SQLITE_OK) {
        cerr << "DB open error: " << sqlite3_errmsg(db) << endl;
        if (db) sqlite3_close(db);
        return;
    }

    //Prepare the SQL statement
    const string sql =
        "SELECT ITEM, SUM(QTY) "
        "FROM Inventory "
        "WHERE DATE(DATETIME) = DATE('now','localtime') "
        "GROUP BY ITEM;";

    rc = sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt, nullptr);
    if (rc != SQLITE_OK) {
        cerr << "Failed to prepare statement: " << sqlite3_errmsg(db) << endl;
        sqlite3_close(db);
        return;
    }

	// Display results
    cout << "Items:\n";

    bool hasData = false;
	//As long as there is a row to fetch, get the item name and quantity, and print them out. Handle any query errors after the loop.
    while ((rc = sqlite3_step(stmt)) == SQLITE_ROW) {
        hasData = true;
        const unsigned char* item = sqlite3_column_text(stmt, 0);
        int qty = sqlite3_column_int(stmt, 1);
		// Reinterpret the item name as a string and print it along with the quantity. If the item name is NULL, print "<NULL>" instead.
        // https://medium.com/@world-of-Finn/how-to-use-sqlite-sdk-in-c-c-62fb47766d20
        cout << (item ? reinterpret_cast<const char*>(item) : "<NULL>") << " | " << qty << "\n";
    }
    if (rc != SQLITE_DONE) {
        cerr << "Query error: " << sqlite3_errmsg(db) << endl;
    }

    if (!hasData) {
        cout << "No records found for today.\n";
    }

    // Cleanup
    sqlite3_finalize(stmt);
    sqlite3_close(db);
    
    cout << "\n" << "Press enter to return to the main screen..." << endl;
}

// Print item name and a symbol for how many times it occuded today
void ItemTracking::GraphItems() {	                                
    // Variables for DB connection and statements         
    sqlite3* db = nullptr;
    sqlite3_stmt* stmt = nullptr;
    int rc;

    vector<string> item_names;
    vector<int> item_counts;

    // Open DB
    rc = sqlite3_open("Inventory.db", &db);
    if (rc != SQLITE_OK) {
        cerr << "DB open error: " << sqlite3_errmsg(db) << endl;
        if (db) sqlite3_close(db);
        return;
    }

    // Prepare the SQL statement
    const char* sql =
        "SELECT ITEM, SUM(QTY) "
        "FROM Inventory "
        "WHERE DATE(DATETIME) = DATE('now','localtime') "
        "GROUP BY ITEM;";

    rc = sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr);
    if (rc != SQLITE_OK) {
        cerr << "Prepare failed: " << sqlite3_errmsg(db) << endl;
        sqlite3_close(db);
        return;
    }

    // Get results
    while ((rc = sqlite3_step(stmt)) == SQLITE_ROW) {
        const unsigned char* item = sqlite3_column_text(stmt, 0);
        int qty = sqlite3_column_int(stmt, 1);

        item_names.push_back(item ? reinterpret_cast<const char*>(item) : "<NULL>");
        item_counts.push_back(qty);
    }

    if (rc != SQLITE_DONE) {
        cerr << "Query error: " << sqlite3_errmsg(db) << endl;
    }
    // No items sold today
    if (item_names.empty()) {
        cout << "\nNo records found for today.\n";
        sqlite3_finalize(stmt);
        sqlite3_close(db);
        return;
    }
    // Display Results
    cout << "\nItem Sales Graph:\n";
    for (size_t i = 0; i < item_names.size(); ++i) {
        cout << item_names[i] << " | ";

        for (int j = 0; j < item_counts[i]; ++j) {
            cout << "$";
        }

        cout << endl;
    }

    // Cleanup
    sqlite3_finalize(stmt);
    sqlite3_close(db);
}

// Gather and print out the top two stats of the day
void ItemTracking::Analytics() {
    sqlite3* db = nullptr;
    sqlite3_stmt* stmt1 = nullptr;
    sqlite3_stmt* stmt2 = nullptr;

    int rc = sqlite3_open("Inventory.db", &db);
    if (rc != SQLITE_OK) {
        cerr << "DB open error: " << sqlite3_errmsg(db) << endl;
        if (db) sqlite3_close(db);
        return;
    }

    // Query 1: Top item sold today
    const char* sql1 =
        "SELECT ITEM, SUM(QTY) as total_qty, SUM(VALUE) as total_value "
        "FROM Inventory "
        "WHERE DATE(DATETIME) = DATE('now','localtime') "
        "GROUP BY ITEM "
        "ORDER BY total_qty DESC "
        "LIMIT 1;";

    rc = sqlite3_prepare_v2(db, sql1, -1, &stmt1, nullptr);
    if (rc != SQLITE_OK) {
        cerr << "Prepare failed (today): " << sqlite3_errmsg(db) << endl;
        sqlite3_close(db);
        return;
    }

    cout << "=== Analytics ===\n";

    if (sqlite3_step(stmt1) == SQLITE_ROW) {
        const unsigned char* item = sqlite3_column_text(stmt1, 0);
        int qty = sqlite3_column_int(stmt1, 1);
        double value = sqlite3_column_double(stmt1, 2);

        cout << "Top Item Today: "
            << (item ? reinterpret_cast<const char*>(item) : "<NULL>")
            << " | Qty: " << qty
            << " | Value: $" << fixed << setprecision(2) << value << "\n";
    }
    else {
        cout << "Top Item Today: No data\n";
    }

    sqlite3_finalize(stmt1);

    // Query 2: Top 3 items last 7 days ---
    const char* sql2 =
        "SELECT ITEM, SUM(QTY) as total_qty, SUM(VALUE) as total_value "
        "FROM Inventory "
        "WHERE DATE(DATETIME) >= DATE('now','-7 days','localtime') "
        "GROUP BY ITEM "
        "ORDER BY total_qty DESC "
        "LIMIT 3;";

    rc = sqlite3_prepare_v2(db, sql2, -1, &stmt2, nullptr);
    if (rc != SQLITE_OK) {
        cerr << "Prepare failed (7 days): " << sqlite3_errmsg(db) << endl;
        sqlite3_close(db);
        return;
    }

    cout << "Top 3 Items (Last 7 Days):\n";

    int rank = 1;
    while (sqlite3_step(stmt2) == SQLITE_ROW) {
        const unsigned char* item = sqlite3_column_text(stmt2, 0);
        int qty = sqlite3_column_int(stmt2, 1);
        double value = sqlite3_column_double(stmt2, 2);

        cout << rank++ << ". "
            << (item ? reinterpret_cast<const char*>(item) : "<NULL>")
            << " | Qty: " << qty
            << " | Value: $" << fixed << setprecision(2) << value << "\n";
    }

    if (rank == 1) {
        cout << "No data\n";
    }

    sqlite3_finalize(stmt2);
    sqlite3_close(db);

    cout << "=================\n\n";
}

// Helper function for libcurl to write response data into a string
static size_t WriteCallback(void* contents, size_t size, size_t nmemb, string* output) {
    output->append((char*)contents, size * nmemb);
    return size * nmemb;
}

// Helper function to extract price from HTML content using regex.
double ExtractPrice(const string& text) {
    regex price_regex(R"(\$([0-9]+\.[0-9]{2}))");
    smatch match;

    // Use regex(https://en.cppreference.com/w/cpp/regex.html) to find price patterns in the HTML. Include user-agent and timeout settings to avoid hanging on bad requests. 
    if (regex_search(text, match, price_regex)) {
        try {
            double p = stod(match[1]);
            if (p > 0.1 && p < 100.0) return p;
        }
        catch (...) {}
    }
    return 0.0;
}

// Helper function to fetch HTML content from a URL using libcurl.
string FetchURL(const string& url) {
    CURL* curl = curl_easy_init();
    if (!curl) return "";

    string buffer;

    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &buffer);

    curl_easy_setopt(curl, CURLOPT_USERAGENT,
        "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 Chrome/120 Safari/537.36");

    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(curl, CURLOPT_ACCEPT_ENCODING, "");

    // Prevent hanging
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 5L);
    curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, 3L);

    CURLcode res = curl_easy_perform(curl);
    curl_easy_cleanup(curl);

    if (res != CURLE_OK) return "";
    return buffer;
}

// Price Scraper to match user input with a web search and extract the price from the HTML.
double ItemTracking::ScrapePrice(const string& itemName) {

    CURL* curl = curl_easy_init();
    if (!curl) return 0.0;

    char* escaped = curl_easy_escape(curl, itemName.c_str(), itemName.length());
    string encodedQuery = escaped ? escaped : itemName;
    if (escaped) curl_free(escaped);
    curl_easy_cleanup(curl);

    // First try Kroger
    string kroger_url = "https://www.kroger.com/search?query=" + encodedQuery;
    string kroger_html = FetchURL(kroger_url);

    double price = ExtractPrice(kroger_html);
    if (price > 0.0) return price;

    // Then DuckDuckGo
    string ddg_url = "https://duckduckgo.com/html/?q=" + encodedQuery + "+price";
    string ddg_html = FetchURL(ddg_url);

    price = ExtractPrice(ddg_html);
    if (price > 0.0) return price;

    return 0.0;
}
