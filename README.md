# 🛒 Smart Shopping Cart System

A full-stack IoT solution that transforms a traditional shopping cart into a smart cart.  
It includes barcode scanning, weight measurement, cloud sync via Firebase, and a web-based management dashboard.

---

## 💡 Overview

This project aims to automate the in-store shopping experience by integrating:
- **ESP32 / Arduino-based hardware** for scanning and weighing products.
- **Flask web application** for managing product data and user interactions.
- **Firebase (Authentication + Firestore + Realtime DB)** for syncing data in real-time.

---

## 📦 Project Structure

```text
smart-shopping-cart/
├── app.py                    # Main Flask application
├── config.py                 # Firebase config and app setup
├── templates/                # HTML pages
│   ├── dashboard.html
│   ├── login.html
│   └── ...
├── static/                   # Static files (CSS, JS, images)
│   ├── css/
│   ├── js/
│   └── images/
├── firebase_admin.json       # Firebase service account (DO NOT share publicly)
├── .env                      # Environment variables (Firebase keys, etc.)
├── requirements.txt          # Python dependencies
└── README.md                 # Project documentation
🚀 Features
🔐 User login system using Firebase Authentication.

📦 Product management (Add/Edit/Delete items).

🧮 Barcode + weight detection via ESP-based devices.

📊 Dashboard UI with product stats and real-time updates.

📤 Automatic data sync with Firebase Firestore / Realtime DB.

🔍 Search functionality to filter products.

🧰 Technologies Used
Layer	Tech
Backend	Python + Flask
Frontend	HTML, CSS, JavaScript
Database	Firebase Firestore + Realtime DB
Auth	Firebase Authentication
Hardware	ESP32, HX711, Barcode Scanner
Network	WiFi (ESP32 to Firebase), Serial

🛠️ Setup Instructions
Clone the repository:


git clone https://github.com/YOUR_USERNAME/smart-shopping-cart.git
cd smart-shopping-cart
(Optional) Create and activate a virtual environment:

python -m venv venv
source venv/bin/activate  # On Windows: venv\Scripts\activate
Install dependencies:

pip install -r requirements.txt
Configure Firebase:

Create a Firebase project.

Download the service account key and save it as firebase_admin.json.

Add your Firebase credentials in .env or inside config.py.

Run the app:

python app.py
🧠 Future Improvements
Integrate payment gateway with points system.

Add user roles (admin/store owner vs customer).

Mobile-friendly responsive UI.

Generate PDF reports for product logs.
  
![image](https://github.com/user-attachments/assets/e330fd5f-5218-4ae6-ae41-5d11706a7bcd)


👨‍💻 Author
Yosef Farhod
📧 yoseffarhod@gmail.com
💡 Embedded + Web Systems Developer
📍 Egypt

📄 License
This project is licensed under the MIT License.

