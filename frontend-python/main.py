#!/usr/bin/env python3
"""
Stock Exchange Trading Dashboard
Main entry point for the PyQt6 application.
"""

import sys
from PyQt6.QtWidgets import QApplication
from PyQt6.QtGui import QFont

from ui import MainWindow


def main():
    app = QApplication(sys.argv)
    
    # Set application-wide font
    font = QFont("Segoe UI", 10)
    app.setFont(font)
    
    # Set dark theme
    app.setStyle("Fusion")
    
    # Create and show main window
    window = MainWindow()
    window.show()
    
    sys.exit(app.exec())


if __name__ == "__main__":
    main()
