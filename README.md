# Sahuji Cashier System

[![Apache License 2.0](https://img.shields.io/badge/License-Apache%202.0-red.svg)](https://www.apache.org/licenses/LICENSE-2.0)
[![GitHub Stars](https://img.shields.io/github/stars/Prarambha369/-SAHUJI-CASHIER.svg?style=social)](https://github.com/Prarambha369/-SAHUJI-CASHIER)
[![HackaTime](https://hackatime-badge.hackclub.com/U0894AG5K1C/-SAHUJI-CASHIER)](https://hackatime.hackclub.com/U0894AG5K1C)
---

## 📋 Table of Contents
- [Overview](#overview)
- [Features](#features)
- [Step-by-Step Usage Guide](#step-by-step-usage-guide)
- [Admin Functions](#admin-functions)
- [User Functions](#user-functions)
- [Changing Admin Password](#changing-admin-password)
- [Security](#security)
- [Data Storage Update (2025)](#data-storage-update-2025)
- [Troubleshooting](#troubleshooting)
- [Screenshots and Videos](#screenshots-and-videos)
- [Contribution Guidelines](#contribution-guidelines)
- [License](#license)

---

## 🛒 Overview
**SAHUJI** is a console-based C++ application designed for small businesses to manage cashier operations. It helps you handle inventory, generate bills, manage customer data, and process credit cards, all from a simple terminal interface.

---

## ✨ Features
- **Product Management:** Add, edit, delete, and view products with details like price, quantity, tax, and discount.
- **Inventory Tracking:** Monitor stock levels and get alerts for low inventory.
- **Billing System:** Generate and view bills for purchases.
- **Customer Management:** Register customers, generate unique customer IDs, and track their purchases and points.
- **Admin Security:** Password-protected admin area with the ability to change the password.
- **Reports:** View product and billing reports.
- **Gift Hamper:** Identify and display the top customer of the year.
- **Cross-Platform:** Works on both Linux and Windows.

---

## 🚀 Step-by-Step Usage Guide

### 1. **Installation**
#### Linux
```sh
sudo apt install g++ libssl-dev      # Install g++ and OpenSSL headers if not already installed
g++ -std=c++17 sahuji.cpp -o sahuji -lcrypto   # Compile the program (recompile after code changes)
./sahuji                            # Run the program
```
#### Windows
- Install a C++ compiler like MinGW and OpenSSL (e.g. Win32/Win64 OpenSSL) so its headers and `libcrypto` are available.
- Compile with: `g++ -std=c++17 sahuji.cpp -o sahuji -lcrypto`
- Run: `./sahuji` or `sahuji.exe` depending on your setup.

### 2. **Main Menu Navigation**
When you start the program, you'll see a menu with the following options:

1. **VIEW/GENERATE CUSTOMER ID**
2. **ADMIN**
3. **USER**
4. **EXIT**

Use the number keys to select an option and follow the on-screen prompts.

---

## 🔐 Admin Functions
Select **ADMIN** from the main menu to use admin features.

- **First run:** you are prompted to create an admin password (minimum 8
  characters, with confirmation). This is required before you can use the
  admin menu — there is no default password.
- The password is stored only as a **salted SHA-256 hash** in `adminpwd.dat`;
  it is never kept in plaintext (see [Security](#security)).

**Admin Menu Options:**
1. Add product
2. View product details (by ID)
3. View all products
4. Edit product
5. Delete product
6. Refill product stock
7. View bills
8. Change Admin Password
9. Back to Main Menu

---

## 👤 User Functions
Select **USER** from the main menu to:
- View available products
- Purchase products (with or without a customer ID)
- Generate bills

---

## 🔑 Changing Admin Password
1. Log in as Admin with the password you created on first run.
2. In the admin menu, select **8. Change Admin Password**.
3. Enter and confirm the new password (minimum 8 characters).
4. The new password is re-salted and re-hashed, then required for future admin logins.

Old versions allowed a default password of `admin` in a plaintext
`adminpwd.txt`; that support has been removed, and any leftover plaintext file
is ignored and deleted once a new hashed password is saved.

---

## 🔒 Security
- The admin password is stored only as `SHA-256(salt || password)` with a
  random 16-byte OpenSSL salt, in `adminpwd.dat`. The plaintext is never
  written to disk.
- First-run setup forces password creation (minimum 8 characters + confirmation).
- See [SECURITY.md](SECURITY.md) for details and hardening notes.

---

## ⚡ Data Storage Update (2025)
- All data is now stored in plain text (CSV) files: `products.dat`, `customers.dat`, and `bills.dat`.
- The admin password is an exception: `adminpwd.dat` holds only a salted SHA-256 hash, never plaintext.
- If you previously used an old version, delete any existing `.dat` files before running the new version to avoid crashes (keep a note of your password — an old `adminpwd.txt`/default `admin` login will no longer work).
- Data is cross-platform and human-readable.

---

## 🐞 Troubleshooting
- If you get a crash or see strange characters in `.dat` files, delete those files and re-add your data.
- Always recompile (`g++ -std=c++17 sahuji.cpp -o sahuji -lcrypto`) after updating the code.
- Forgot your admin password? Delete `adminpwd.dat` and restart the app to set a new one.

---

## 🖼️ Screenshots and Videos

- [Video](https://hc-cdn.hel1.your-objectstorage.com/s/v3/36e0dd2a5c46fef6a8141f9dc83062cd44e5cfa5_sahuji_app_run.mp4)

---

## 🤝 Contribution Guidelines
- Fork the repository.
- Make changes and submit a pull request.
- Ensure code is cross-platform compatible.
- Report issues on the GitHub page if available.

---

## 📄 License
This project is licensed under the [Apache License 2.0](LICENSE).

---

**Enjoy using Sahuji Cashier System! made with <3 by Praarmbha**
