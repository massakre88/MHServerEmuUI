# **MHServerEmuUI**

<<<<<<< HEAD
### **A Modern GUI for Simplifying MHServerEmu Management**

MHServerEmuUI provides a streamlined and intuitive interface for managing the MHServerEmu game server. Designed with ease of use in mind, this GUI consolidates all essential server operations into one powerful tool.
=======
### **A GUI for Simplifying MHServerEmu Management**

MHServerEmuUI provides an interface for managing the MHServerEmu game server. Designed with ease of use in mind, this GUI consolidates many essential server operations into one tool.
>>>>>>> bf937cc84d9ed903d74b0b1fd4643e3ea7a53e4c

---

## **Features**
- **User-Friendly Interface**: Manage your server effortlessly with a clear and organized GUI.
- **Event Management**: Toggle special events like Cosmic Chaos and Armor Incursion with just a click.
- **Live Tuning Adjustments**: Easily modify game settings in real-time, including XP rates, loot drop chances, and more.
<<<<<<< HEAD
- **Nightly Updates**: Automatically download and apply the latest server updates.
- **Backup Management**: Ensure critical files like `config.ini` and live tuning data are preserved during updates.
- **Broadcast Messaging**: Send server-wide announcements to keep players informed.
=======
- **Nightly Updates**: One button click to download and apply the latest server updates.
- **Backup Management**: Ensure critical files like `config.ini`, live tuning data and custom store are preserved during updates.
- **Broadcast Messaging**: Send server-wide announcements to keep players informed.
- **Server shutdown timer**: Send a server-wide message that the server is going to shutdown with the number of minutes until it happens.
>>>>>>> bf937cc84d9ed903d74b0b1fd4643e3ea7a53e4c

---

## **Requirements**
- **MHServerEmu**: Ensure you have the latest **stable release** of MHServerEmu installed. You can download it [here](https://github.com/Crypto137/MHServerEmu).
- **Qt Framework**: This project is built using Qt Creator. You can download the latest Qt version from [qt.io](https://www.qt.io/).
- **cURL**: For handling nightly updates, make sure cURL is installed and available in your system's PATH (Windows 10/11 includes cURL by default).

---

## **Installation & Setup**
1. **Prepare MHServerEmu**:
   - Download and extract the latest stable release of MHServerEmu.
   - Run the **setup tool** provided with MHServerEmu to initialize the server.

2. **Set Up MHServerEmuUI**:
<<<<<<< HEAD
   - Clone this repository or download the latest release from [GitHub](https://github.com/Pyrox37/MHServerEmuUI).
=======
   - Clone this repository or download the latest release from [GitHub](https://github.com/Pyrox37/MHServerEmuUI/releases).
>>>>>>> bf937cc84d9ed903d74b0b1fd4643e3ea7a53e4c
   - Build the project using **Qt Creator** (or use the provided prebuilt executable).

3. **First Run**:
   - Launch MHServerEmuUI.
   - In the **MHServerEmu Folder** text box, set the location of your extracted server folder (e.g., `C:\MHServerEmu-0.4.0`).

---

## **Usage Guide**
### **Key Features**
<<<<<<< HEAD
- **Server Control**: Start, stop, and monitor your server with ease. Server shutdown with a timer and broadcast messages.
=======
- **Server Control**: Start, stop, and monitor your server with ease.
>>>>>>> bf937cc84d9ed903d74b0b1fd4643e3ea7a53e4c
- **Event Toggles**: Enable or disable game events like:
  - **Cosmic Chaos**
  - **Armor Incursion**
  - **Midtown Madness**
- **Live Tuning Updates**: Adjust server settings in the `LiveTuningData.json` file dynamically.
- **Easy Updates**:
  - Download and apply the latest server builds.
  - Preserve critical files during updates (e.g., `config.ini`, live tuning data, and custom store files).
- **Broadcast Messaging**:
  - Send player notifications during events or server shutdowns.

---

## **Acknowledgments**
- **MHServerEmu**: The backbone of this project.
- **Qt Framework**: For providing the tools to create this GUI.
- **cURL**: For enabling seamless downloads of nightly updates.
