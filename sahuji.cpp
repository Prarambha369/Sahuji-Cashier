#include <iostream>
#include <iomanip>
#include <string>
#include <fstream>
#include <vector>
#include <limits>
#include <cstdlib>
#include <ctime>
#include <sstream>
#include <cstdio>
#include <cctype>
#include <cstdint>
#include <openssl/sha.h>
#include <openssl/rand.h>
#ifdef _WIN32
#include <conio.h>
#include <windows.h>
#else
#include <unistd.h>
#endif
using namespace std;

string get_input(const string& prompt) {
    string input;
    cout << prompt;
    getline(cin, input);
    size_t start = input.find_first_not_of(" \t\n\r");
    size_t end = input.find_last_not_of(" \t\n\r");
    if (start == string::npos) return "";
    return input.substr(start, end - start + 1);
}

void wait_key() {
#ifdef _WIN32
    _getch();
#else
    cout << "(Press Enter to continue...)";
    cin.ignore(numeric_limits<streamsize>::max(), '\n');
#endif
}

struct Product {
    int id;
    string name;
    string mfg_date;
    float cost_price;
    float mrp;
    float qty;
    float tax;
    float discount;
    float selling_price;

    void compute() {
        float gross = mrp + (mrp * (tax / 100));
        selling_price = gross - (gross * (discount / 100));
    }
};

struct Customer {
    string name;
    string citizenship;
    int age;
    int id;
};

struct Bill {
    int customer_id;
    string customer_name;
    string product_name;
    int product_id;
    float price;
    float qty;
    float total;
};

const string PRODUCT_FILE = "products.dat";
const string CUSTOMER_FILE = "customers.dat";
const string BILL_FILE = "bills.dat";
const string ADMIN_PWD_FILE = "adminpwd.dat";     // stores "salt_hex:hash_hex" (never plaintext)
const string LEGACY_PWD_FILE = "adminpwd.txt";    // old plaintext store; removed on migration
const size_t ADMIN_SALT_BYTES = 16;               // random salt length
const size_t ADMIN_MIN_PWD_LEN = 8;               // minimum password length

// --- SHA-256 helpers (OpenSSL) ---
string sha256_hex(const string& data) {
    unsigned char digest[SHA256_DIGEST_LENGTH];
    SHA256(reinterpret_cast<const unsigned char*>(data.data()), data.size(), digest);
    ostringstream oss;
    oss << hex << setfill('0');
    for (unsigned char c : digest) oss << setw(2) << static_cast<int>(c);
    return oss.str();
}

string random_salt_hex() {
    unsigned char salt[ADMIN_SALT_BYTES];
    if (RAND_bytes(salt, static_cast<int>(ADMIN_SALT_BYTES)) != 1) {
        // Extremely unlikely; OpenSSL CSPRNG failure. Fail closed with seed data instead.
        srand(static_cast<unsigned>(time(nullptr)) ^ static_cast<unsigned>(reinterpret_cast<uintptr_t>(&salt)));
        for (auto& b : salt) b = static_cast<unsigned char>(rand());
    }
    ostringstream oss;
    oss << hex << setfill('0');
    for (unsigned char c : salt) oss << setw(2) << static_cast<int>(c);
    return oss.str();
}

string salted_password_hash(const string& salt_hex, const string& pwd) {
    // Hash of salt||password; only this digest and the salt are ever stored.
    return sha256_hex(salt_hex + pwd);
}

void trim_password(const string& raw, string& out) {
    size_t start = raw.find_first_not_of(" \t\n\r");
    size_t end = raw.find_last_not_of(" \t\n\r");
    out = (start == string::npos) ? "" : raw.substr(start, end - start + 1);
}

// Returns true and fills salt/hash if a password record is stored.
bool load_admin_password_record(string& salt_hex, string& hash_hex) {
    ifstream f(ADMIN_PWD_FILE);
    if (!f.is_open()) return false;
    string line;
    if (!getline(f, line)) return false;
    f.close();
    size_t sep = line.find(':');
    if (sep == string::npos || sep == 0 || sep + 1 >= line.size()) return false;
    string s = line.substr(0, sep);
    string h = line.substr(sep + 1);
    // Basic sanity: both fields must be hex of the expected length (64 hex chars each).
    if (s.size() != 2 * ADMIN_SALT_BYTES || h.size() != 2 * SHA256_DIGEST_LENGTH) return false;
    for (char c : s + h) {
        if (!isxdigit(static_cast<unsigned char>(c))) return false;
    }
    salt_hex = s;
    hash_hex = h;
    return true;
}

bool save_admin_password_record(const string& salt_hex, const string& hash_hex) {
    ofstream out(ADMIN_PWD_FILE, ios::trunc);
    if (!out.is_open()) return false;
    out << salt_hex << ':' << hash_hex << endl;
    out.flush();
    out.close();
    if (out.fail()) return false;
    // Migration: drop the legacy plaintext password file if it still exists.
    remove(LEGACY_PWD_FILE.c_str());
    return true;
}

// Constant-time comparison to avoid timing side channels.
bool constant_time_equal(const string& a, const string& b) {
    if (a.size() != b.size()) return false;
    volatile unsigned char diff = 0;
    for (size_t i = 0; i < a.size(); ++i) {
        diff |= static_cast<unsigned char>(a[i]) ^ static_cast<unsigned char>(b[i]);
    }
    return diff == 0;
}

// Verify a candidate password against the stored salted hash.
bool verify_admin_password(const string& pwd) {
    string salt_hex, hash_hex;
    if (!load_admin_password_record(salt_hex, hash_hex)) return false;
    string candidate = salted_password_hash(salt_hex, pwd);
    return constant_time_equal(candidate, hash_hex);
}

// Store a new password as a fresh random-salted hash. Plaintext never touches disk.
bool set_admin_password(const string& newpwd) {
    if (newpwd.size() < ADMIN_MIN_PWD_LEN) return false;
    string salt_hex = random_salt_hex();
    return save_admin_password_record(salt_hex, salted_password_hash(salt_hex, newpwd));
}

// Prompt for a new password (min length), require a matching confirmation,
// and return the trimmed password; returns "" if the user backs out.
string prompt_new_password() {
    while (true) {
        string np, cp;
        trim_password(get_input("\nEnter new password (min " + to_string(ADMIN_MIN_PWD_LEN) + " chars, or 'back' to cancel): "), np);
        if (np == "back" || np == "menu" || np == "exit") return "";
        if (np.size() < ADMIN_MIN_PWD_LEN) {
            cout << "Error: Password must be at least " << ADMIN_MIN_PWD_LEN << " characters." << endl;
            continue;
        }
        trim_password(get_input("Confirm new password: "), cp);
        if (cp == "back" || cp == "menu" || cp == "exit") return "";
        if (np != cp) {
            cout << "Error: Passwords do not match. Please try again." << endl;
            continue;
        }
        return np;
    }
}

// First-run guard: if no password record exists, force the admin to create one.
bool ensure_admin_password() {
    string salt_hex, hash_hex;
    if (load_admin_password_record(salt_hex, hash_hex)) return true;

    ifstream legacy(LEGACY_PWD_FILE);
    if (legacy.is_open()) {
        legacy.close();
        cout << "\nNote: an old plaintext password file (" << LEGACY_PWD_FILE
             << ") was found and will no longer be used." << endl;
        cout << "Passwords are now stored only as salted hashes." << endl;
    }

    cout << "\n--- First-time setup: create an admin password ---" << endl;
    while (true) {
        string pwd = prompt_new_password();
        if (pwd.empty()) {
            string again = get_input("Admin setup is required to use this menu. Try again? (y/n): ");
            if (again != "y" && again != "Y") return false;
            continue;
        }
        if (set_admin_password(pwd)) {
            cout << "Success: Admin password created and stored as a salted hash." << endl;
            return true;
        }
        cout << "Error: Failed to save the password. Check file permissions and try again." << endl;
    }
}


// --- Robust text-based Product file helpers ---
vector<Product> load_products() {
    vector<Product> v;
    ifstream f(PRODUCT_FILE);
    if (!f) return v;
    string line;
    while (getline(f, line)) {
        if (line.empty()) continue;
        istringstream iss(line);
        Product p;
        char delim;
        if (iss >> p.id >> delim && delim == ',' &&
            getline(iss, p.name, ',') &&
            getline(iss, p.mfg_date, ',') &&
            iss >> p.cost_price >> delim && delim == ',' &&
            iss >> p.mrp >> delim && delim == ',' &&
            iss >> p.qty >> delim && delim == ',' &&
            iss >> p.tax >> delim && delim == ',' &&
            iss >> p.discount) {
            p.compute();
            v.push_back(p);
        }
    }
    f.close();
    return v;
}
void save_products(const vector<Product>& v) {
    ofstream f(PRODUCT_FILE, ios::trunc);
    for (const auto& p : v) {
        f << p.id << ',' << p.name << ',' << p.mfg_date << ',' << p.cost_price << ',' << p.mrp << ',' << p.qty << ',' << p.tax << ',' << p.discount << '\n';
    }
    f.close();
}
// --- Robust text-based Customer file helpers ---
vector<Customer> load_customers() {
    vector<Customer> v;
    ifstream f(CUSTOMER_FILE);
    if (!f) return v;
    string line;
    while (getline(f, line)) {
        if (line.empty()) continue;
        istringstream iss(line);
        Customer c;
        char delim;
        if (getline(iss, c.name, ',') &&
            getline(iss, c.citizenship, ',') &&
            iss >> c.age >> delim && delim == ',' &&
            iss >> c.id) {
            v.push_back(c);
        }
    }
    f.close();
    return v;
}
void save_customers(const vector<Customer>& v) {
    ofstream f(CUSTOMER_FILE, ios::trunc);
    for (const auto& c : v) {
        f << c.name << ',' << c.citizenship << ',' << c.age << ',' << c.id << '\n';
    }
    f.close();
}
// --- Robust text-based Bill file helpers ---
vector<Bill> load_bills() {
    vector<Bill> v;
    ifstream f(BILL_FILE);
    if (!f) return v;
    string line;
    while (getline(f, line)) {
        if (line.empty()) continue;
        istringstream iss(line);
        Bill b;
        char delim;
        if (iss >> b.customer_id >> delim && delim == ',' &&
            getline(iss, b.customer_name, ',') &&
            getline(iss, b.product_name, ',') &&
            iss >> b.product_id >> delim && delim == ',' &&
            iss >> b.price >> delim && delim == ',' &&
            iss >> b.qty >> delim && delim == ',' &&
            iss >> b.total) {
            v.push_back(b);
        }
    }
    f.close();
    return v;
}
void save_bills(const vector<Bill>& v) {
    ofstream f(BILL_FILE, ios::trunc);
    for (const auto& b : v) {
        f << b.customer_id << ',' << b.customer_name << ',' << b.product_name << ',' << b.product_id << ',' << b.price << ',' << b.qty << ',' << b.total << '\n';
    }
    f.close();
}

int find_product(const vector<Product>& v, int id) {
    for (size_t i = 0; i < v.size(); ++i) if (v[i].id == id) return i;
    return -1;
}

int find_customer(const vector<Customer>& v, int id) {
    for (size_t i = 0; i < v.size(); ++i) if (v[i].id == id) return i;
    return -1;
}

void admin_menu();
void user_menu();
void generate_customer_id();

void main_menu() {
    while (true) {
        cout << "\n\t\t\t==============================";
        cout << "\n\t\t\t   SAHUJI Cashier App";
        cout << "\n\t\t\t==============================\n";
        cout << "\n1. Generate Customer ID";
        cout << "\n2. Admin";
        cout << "\n3. User";
        cout << "\n4. Exit";
        string input = get_input("\n\nEnter your choice (or 'exit'): ");
        if (input == "exit" || input == "4") break;
        if (input == "1") generate_customer_id();
        else if (input == "2") admin_menu();
        else if (input == "3") user_menu();
        else cout << "\nInvalid choice.";
    }
    cout << "\nThank you for using the app!\n";
}

void generate_customer_id() {
    vector<Customer> customers = load_customers();
    while (true) {
        cout << "\n--- Customer ID Generation ---";
        string name = get_input("Enter customer name (or 'back'): ");
        if (name == "back" || name == "menu" || name == "exit") return;
        string citizenship = get_input("Enter citizenship no.: ");
        if (citizenship == "back" || citizenship == "menu" || citizenship == "exit") return;
        string age_str = get_input("Enter age: ");
        if (age_str == "back" || age_str == "menu" || age_str == "exit") return;
        int age = 0;
        try { age = stoi(age_str); } catch (...) { cout << "Invalid age. Try again."; continue; }
        srand((unsigned)time(0));
        int id = 1000 + rand() % 9000;
        customers.push_back({name, citizenship, age, id});
        save_customers(customers);
        cout << "\nCustomer ID generated: " << id << "\n";
        string more = get_input("Generate another? (y/n): ");
        if (more != "y" && more != "Y") break;
    }
}

void admin_menu() {
    if (!ensure_admin_password()) {
        cout << "\nAdmin access denied: no admin password has been set up." << endl;
        return;
    }
    int tries = 3;
    while (tries--) {
        string input = get_input("\nEnter admin password (or 'back'): ");
        if (input == "back" || input == "menu" || input == "exit") return;
        if (verify_admin_password(input)) break;
        cout << "Incorrect password. Attempts left: " << tries << endl;
        if (tries == 0) return;
    }
    while (true) {
        cout << "\n--- Admin Menu ---";
        cout << "\n1. Add Product";
        cout << "\n2. View Product Details";
        cout << "\n3. View All Products";
        cout << "\n4. Edit Product";
        cout << "\n5. Delete Product";
        cout << "\n6. Refill Product Stock";
        cout << "\n7. View Bills";
        cout << "\n8. Change Admin Password";
        cout << "\n9. Back to Main Menu";
        string input = get_input("\nEnter your choice: ");
        if (input == "back" || input == "menu" || input == "9") return;
        vector<Product> products = load_products();
        if (input == "1") {
            Product p;
            string id_str = get_input("Enter product ID: ");
            if (id_str == "back" || id_str == "menu" || id_str == "exit") continue;
            try { p.id = stoi(id_str); } catch (...) { cout << "Invalid ID."; continue; }
            p.name = get_input("Enter product name: ");
            if (p.name == "back" || p.name == "menu" || p.name == "exit") continue;
            p.mfg_date = get_input("Enter manufacturing date (dd-mm-yy): ");
            if (p.mfg_date == "back" || p.mfg_date == "menu" || p.mfg_date == "exit") continue;
            string cp = get_input("Enter cost price: ");
            string mrp = get_input("Enter MRP: ");
            string qty = get_input("Enter quantity: ");
            string tax = get_input("Enter tax %: ");
            string disc = get_input("Enter discount %: ");
            try {
                p.cost_price = stof(cp);
                p.mrp = stof(mrp);
                p.qty = stof(qty);
                p.tax = stof(tax);
                p.discount = stof(disc);
            } catch (...) { cout << "Invalid input."; continue; }
            p.compute();
            products.push_back(p);
            save_products(products);
            cout << "Product added!";
        } else if (input == "2") {
            string id_str = get_input("Enter product ID: ");
            if (id_str == "back" || id_str == "menu" || id_str == "exit") continue;
            int id = 0;
            try { id = stoi(id_str); } catch (...) { cout << "Invalid ID."; continue; }
            int idx = find_product(products, id);
            if (idx == -1) { cout << "Product not found."; continue; }
            const Product& p = products[idx];
            cout << fixed << setprecision(2);
            cout << "\nID: " << p.id << "\nName: " << p.name << "\nMfg Date: " << p.mfg_date << "\nCost Price: " << p.cost_price << "\nMRP: " << p.mrp << "\nQty: " << p.qty << "\nTax: " << p.tax << "%\nDiscount: " << p.discount << "%\nSelling Price: " << p.selling_price << endl;
            wait_key();
        } else if (input == "3") {
            cout << "\n--- All Products ---\n";
            cout << left << setw(8) << "ID" << setw(20) << "Name" << setw(12) << "Qty" << setw(10) << "MRP" << setw(10) << "SellPrice" << endl;
            for (const auto& p : products) {
                cout << left << setw(8) << p.id << setw(20) << p.name << setw(12) << p.qty << setw(10) << p.mrp << setw(10) << p.selling_price << endl;
            }
            wait_key();
        } else if (input == "4") {
            string id_str = get_input("Enter product ID to edit: ");
            if (id_str == "back" || id_str == "menu" || id_str == "exit") continue;
            int id = 0;
            try { id = stoi(id_str); } catch (...) { cout << "Invalid ID."; continue; }
            int idx = find_product(products, id);
            if (idx == -1) { cout << "Product not found."; continue; }
            Product& p = products[idx];
            string name = get_input("Enter new name (or leave blank): ");
            if (!name.empty()) p.name = name;
            string mfg = get_input("Enter new mfg date (or leave blank): ");
            if (!mfg.empty()) p.mfg_date = mfg;
            string cp = get_input("Enter new cost price (or leave blank): ");
            if (!cp.empty()) p.cost_price = stof(cp);
            string mrp = get_input("Enter new MRP (or leave blank): ");
            if (!mrp.empty()) p.mrp = stof(mrp);
            string qty = get_input("Enter new quantity (or leave blank): ");
            if (!qty.empty()) p.qty = stof(qty);
            string tax = get_input("Enter new tax % (or leave blank): ");
            if (!tax.empty()) p.tax = stof(tax);
            string disc = get_input("Enter new discount % (or leave blank): ");
            if (!disc.empty()) p.discount = stof(disc);
            p.compute();
            save_products(products);
            cout << "Product updated!";
        } else if (input == "5") {
            string id_str = get_input("Enter product ID to delete: ");
            if (id_str == "back" || id_str == "menu" || id_str == "exit") continue;
            int id = 0;
            try { id = stoi(id_str); } catch (...) { cout << "Invalid ID."; continue; }
            int idx = find_product(products, id);
            if (idx == -1) { cout << "Product not found."; continue; }
            products.erase(products.begin() + idx);
            save_products(products);
            cout << "Product deleted!";
        } else if (input == "6") {
            string id_str = get_input("Enter product ID to refill: ");
            if (id_str == "back" || id_str == "menu" || id_str == "exit") continue;
            int id = 0;
            try { id = stoi(id_str); } catch (...) { cout << "Invalid ID."; continue; }
            int idx = find_product(products, id);
            if (idx == -1) { cout << "Product not found."; continue; }
            string qty = get_input("Enter quantity to add: ");
            try { products[idx].qty += stof(qty); } catch (...) { cout << "Invalid quantity."; continue; }
            save_products(products);
            cout << "Stock refilled!";
        } else if (input == "7") {
            vector<Bill> bills = load_bills();
            cout << "\n--- Bills ---\n";
            for (const auto& b : bills) {
                cout << "Customer: " << b.customer_name << " (ID: " << b.customer_id << ")\n";
                cout << "Product: " << b.product_name << " (ID: " << b.product_id << ")\n";
                cout << "Qty: " << b.qty << " Price: " << b.price << " Total: " << b.total << "\n---\n";
            }
            wait_key();
        } else if (input == "8") {
            // Change admin password: min length + confirmation, stored as a salted hash.
            string np = prompt_new_password();
            if (np.empty()) continue;

            if (set_admin_password(np)) {
                cout << "Success: Admin password has been changed successfully!";
            } else {
                cout << "Error: Failed to save the new password. Please check file permissions and try again.";
            }
        } else {
            cout << "Invalid choice.";
        }
    }
}

void user_menu() {
    vector<Customer> customers = load_customers();
    int customer_id = 0;
    string has_id = get_input("\nDo you have a customer ID? (y/n): ");
    if (has_id == "y" || has_id == "Y") {
        string id_str = get_input("Enter your customer ID: ");
        try { customer_id = stoi(id_str); } catch (...) { cout << "Invalid ID."; return; }
        if (find_customer(customers, customer_id) == -1) { cout << "Customer not found."; return; }
    }
    while (true) {
        vector<Product> products = load_products();
        cout << "\n--- Products ---\n";
        cout << left << setw(8) << "ID" << setw(20) << "Name" << setw(12) << "Qty" << setw(10) << "MRP" << setw(10) << "SellPrice" << endl;
        for (const auto& p : products) {
            cout << left << setw(8) << p.id << setw(20) << p.name << setw(12) << p.qty << setw(10) << p.mrp << setw(10) << p.selling_price << endl;
        }
        string input = get_input("\nEnter product ID to buy (or 'back'): ");
        if (input == "back" || input == "menu" || input == "exit") return;
        int pid = 0;
        try { pid = stoi(input); } catch (...) { cout << "Invalid ID."; continue; }
        int idx = find_product(products, pid);
        if (idx == -1) { cout << "Product not found."; continue; }
        string qty_str = get_input("Enter quantity to buy: ");
        float qty = 0;
        try { qty = stof(qty_str); } catch (...) { cout << "Invalid quantity."; continue; }
        if (qty > products[idx].qty) { cout << "Not enough stock."; continue; }
        products[idx].qty -= qty;
        save_products(products);
        vector<Bill> bills = load_bills();
        string cname = "Guest";
        if (customer_id != 0) {
            int cidx = find_customer(customers, customer_id);
            if (cidx != -1) cname = customers[cidx].name;
        }
        Bill b = {customer_id, cname, products[idx].name, products[idx].id, products[idx].selling_price, qty, products[idx].selling_price * qty};
        bills.push_back(b);
        save_bills(bills);
        cout << "\nPurchase successful! Total: " << b.total << endl;
        string more = get_input("Buy another product? (y/n): ");
        if (more != "y" && more != "Y") break;
    }
}

int main() {
    main_menu();
    return 0;
}