"""
Main window for the Trading Dashboard.
"""

from PyQt6.QtWidgets import (
    QMainWindow, QWidget, QVBoxLayout, QHBoxLayout, QSplitter,
    QGroupBox, QFormLayout, QLineEdit, QComboBox, QPushButton,
    QTableWidget, QTableWidgetItem, QTextEdit, QLabel, QStatusBar,
    QHeaderView, QSpinBox, QDoubleSpinBox
)
from PyQt6.QtCore import Qt, QTimer
from PyQt6.QtGui import QFont, QColor

from charts import CandlestickChart, VolumeChart, OrderBookDepth, OrderBookTable
from core import SocketClient, DataParser


class EventLog(QTextEdit):
    """Scrolling event log widget."""
    
    def __init__(self, parent=None):
        super().__init__(parent)
        self.setReadOnly(True)
        self.setMaximumHeight(150)
        self.setStyleSheet("""
            QTextEdit {
                background-color: #11111b;
                color: #cdd6f4;
                border: 1px solid #313244;
                border-radius: 4px;
                font-family: 'Consolas', 'Courier New', monospace;
                font-size: 11px;
            }
        """)
    
    def log(self, message: str, level: str = "INFO"):
        """Add a log message."""
        colors = {
            "INFO": "#89b4fa",
            "SUCCESS": "#a6e3a1",
            "WARNING": "#f9e2af",
            "ERROR": "#f38ba8"
        }
        color = colors.get(level, "#cdd6f4")
        
        import datetime
        timestamp = datetime.datetime.now().strftime("%H:%M:%S")
        self.append(f'<span style="color: #6c7086;">[{timestamp}]</span> '
                   f'<span style="color: {color};">{message}</span>')
        
        # Scroll to bottom
        scrollbar = self.verticalScrollBar()
        scrollbar.setValue(scrollbar.maximum())


class PortfolioTable(QTableWidget):
    """Portfolio positions table."""
    
    def __init__(self, parent=None):
        super().__init__(parent)
        self.setColumnCount(5)
        self.setHorizontalHeaderLabels(["Ticker", "Qty", "Avg Cost", "P&L", "Value"])
        self.horizontalHeader().setSectionResizeMode(QHeaderView.ResizeMode.Stretch)
        self.setAlternatingRowColors(True)
        self.setStyleSheet("""
            QTableWidget {
                background-color: #1e1e2e;
                color: #cdd6f4;
                border: 1px solid #313244;
                gridline-color: #313244;
            }
            QHeaderView::section {
                background-color: #313244;
                color: #cdd6f4;
                padding: 5px;
                border: none;
                font-weight: bold;
            }
            QTableWidget::item:alternate {
                background-color: #181825;
            }
        """)
        self.positions = {}
    
    def update_position(self, ticker: str, qty: int, avg_cost: float, 
                        current_price: float):
        """Update a portfolio position."""
        value = qty * current_price
        pnl = (current_price - avg_cost) * qty
        
        self.positions[ticker] = {
            'qty': qty,
            'avg_cost': avg_cost,
            'pnl': pnl,
            'value': value
        }
        
        self._refresh_table()
    
    def _refresh_table(self):
        """Refresh the table display."""
        self.setRowCount(len(self.positions))
        
        for row, (ticker, pos) in enumerate(self.positions.items()):
            self.setItem(row, 0, QTableWidgetItem(ticker))
            self.setItem(row, 1, QTableWidgetItem(str(pos['qty'])))
            self.setItem(row, 2, QTableWidgetItem(f"${pos['avg_cost']:.2f}"))
            
            pnl_item = QTableWidgetItem(f"${pos['pnl']:+.2f}")
            pnl_item.setForeground(QColor("#a6e3a1" if pos['pnl'] >= 0 else "#f38ba8"))
            self.setItem(row, 3, pnl_item)
            
            self.setItem(row, 4, QTableWidgetItem(f"${pos['value']:.2f}"))


class TradingPanel(QGroupBox):
    """Trading controls panel."""
    
    def __init__(self, parent=None):
        super().__init__("Trading", parent)
        self.socket_client = None
        self._setup_ui()
    
    def _setup_ui(self):
        self.setStyleSheet("""
            QGroupBox {
                color: #cdd6f4;
                font-weight: bold;
                border: 1px solid #313244;
                border-radius: 4px;
                margin-top: 10px;
                padding-top: 10px;
            }
            QGroupBox::title {
                subcontrol-origin: margin;
                left: 10px;
                padding: 0 5px;
            }
        """)
        
        layout = QFormLayout(self)
        layout.setSpacing(8)
        
        # Ticker selection
        self.ticker_combo = QComboBox()
        self.ticker_combo.addItems(["AAPL", "MSFT", "GOOGL", "AMZN", "TSLA"])
        self.ticker_combo.setStyleSheet("""
            QComboBox {
                background-color: #313244;
                color: #cdd6f4;
                border: 1px solid #45475a;
                border-radius: 4px;
                padding: 5px;
            }
        """)
        layout.addRow("Ticker:", self.ticker_combo)
        
        # Side selection
        self.side_combo = QComboBox()
        self.side_combo.addItems(["BUY", "SELL"])
        self.side_combo.setStyleSheet(self.ticker_combo.styleSheet())
        layout.addRow("Side:", self.side_combo)
        
        # Order type
        self.type_combo = QComboBox()
        self.type_combo.addItems(["LIMIT", "MARKET"])
        self.type_combo.setStyleSheet(self.ticker_combo.styleSheet())
        self.type_combo.currentTextChanged.connect(self._on_type_changed)
        layout.addRow("Type:", self.type_combo)
        
        # Price
        self.price_spin = QDoubleSpinBox()
        self.price_spin.setRange(0.01, 100000.0)
        self.price_spin.setDecimals(2)
        self.price_spin.setValue(100.0)
        self.price_spin.setStyleSheet("""
            QDoubleSpinBox {
                background-color: #313244;
                color: #cdd6f4;
                border: 1px solid #45475a;
                border-radius: 4px;
                padding: 5px;
            }
        """)
        layout.addRow("Price:", self.price_spin)
        
        # Quantity
        self.qty_spin = QSpinBox()
        self.qty_spin.setRange(1, 100000)
        self.qty_spin.setValue(100)
        self.qty_spin.setStyleSheet(self.price_spin.styleSheet())
        layout.addRow("Quantity:", self.qty_spin)
        
        # Submit button
        self.submit_btn = QPushButton("Submit Order")
        self.submit_btn.setStyleSheet("""
            QPushButton {
                background-color: #89b4fa;
                color: #1e1e2e;
                border: none;
                border-radius: 4px;
                padding: 10px;
                font-weight: bold;
            }
            QPushButton:hover {
                background-color: #b4befe;
            }
            QPushButton:pressed {
                background-color: #74c7ec;
            }
        """)
        self.submit_btn.clicked.connect(self._submit_order)
        layout.addRow(self.submit_btn)
    
    def set_socket_client(self, client: SocketClient):
        """Set the socket client."""
        self.socket_client = client
    
    def _on_type_changed(self, order_type: str):
        """Handle order type change."""
        self.price_spin.setEnabled(order_type == "LIMIT")
    
    def _submit_order(self):
        """Submit the order."""
        if not self.socket_client:
            return
        
        ticker = self.ticker_combo.currentText()
        side = self.side_combo.currentText()
        order_type = self.type_combo.currentText()
        price = self.price_spin.value() if order_type == "LIMIT" else 0.0
        quantity = self.qty_spin.value()
        
        self.socket_client.send_order(ticker, side, order_type, price, quantity)


class MainWindow(QMainWindow):
    """Main trading dashboard window."""
    
    def __init__(self):
        super().__init__()
        
        self.socket_client = SocketClient()
        self.data_parser = DataParser()
        self.current_ticker = "AAPL"
        self.market_data_cache = {}
        
        self._setup_ui()
        self._connect_signals()
        
        # Auto-connect on startup
        QTimer.singleShot(500, self._connect_to_server)
    
    def _setup_ui(self):
        self.setWindowTitle("Stock Exchange Trading Dashboard")
        self.setMinimumSize(1400, 900)
        self.setStyleSheet("""
            QMainWindow {
                background-color: #1e1e2e;
            }
            QWidget {
                color: #cdd6f4;
            }
        """)
        
        central = QWidget()
        self.setCentralWidget(central)
        
        main_layout = QHBoxLayout(central)
        main_layout.setContentsMargins(10, 10, 10, 10)
        main_layout.setSpacing(10)
        
        # Left panel (charts)
        left_panel = QWidget()
        left_layout = QVBoxLayout(left_panel)
        left_layout.setContentsMargins(0, 0, 0, 0)
        left_layout.setSpacing(5)
        
        # Ticker selector
        ticker_layout = QHBoxLayout()
        ticker_label = QLabel("Symbol:")
        ticker_label.setStyleSheet("font-weight: bold;")
        
        self.main_ticker_combo = QComboBox()
        self.main_ticker_combo.addItems(["AAPL", "MSFT", "GOOGL", "AMZN", "TSLA"])
        self.main_ticker_combo.setStyleSheet("""
            QComboBox {
                background-color: #313244;
                color: #cdd6f4;
                border: 1px solid #45475a;
                border-radius: 4px;
                padding: 8px 15px;
                font-size: 14px;
                min-width: 100px;
            }
        """)
        self.main_ticker_combo.currentTextChanged.connect(self._on_ticker_changed)
        
        self.price_label = QLabel("$0.00")
        self.price_label.setStyleSheet("font-size: 24px; font-weight: bold; color: #a6e3a1;")
        
        self.change_label = QLabel("+0.00 (0.00%)")
        self.change_label.setStyleSheet("font-size: 14px; color: #6c7086;")
        
        ticker_layout.addWidget(ticker_label)
        ticker_layout.addWidget(self.main_ticker_combo)
        ticker_layout.addWidget(self.price_label)
        ticker_layout.addWidget(self.change_label)
        ticker_layout.addStretch()
        
        left_layout.addLayout(ticker_layout)
        
        # Candlestick chart
        self.candlestick_chart = CandlestickChart()
        left_layout.addWidget(self.candlestick_chart, stretch=4)
        
        # Volume chart
        self.volume_chart = VolumeChart()
        left_layout.addWidget(self.volume_chart, stretch=1)
        
        # Event log
        log_label = QLabel("Event Log")
        log_label.setStyleSheet("font-weight: bold; margin-top: 10px;")
        left_layout.addWidget(log_label)
        
        self.event_log = EventLog()
        left_layout.addWidget(self.event_log)
        
        # Right panel
        right_panel = QWidget()
        right_panel.setMaximumWidth(400)
        right_layout = QVBoxLayout(right_panel)
        right_layout.setContentsMargins(0, 0, 0, 0)
        right_layout.setSpacing(10)
        
        # Order book
        self.order_book_table = OrderBookTable()
        right_layout.addWidget(self.order_book_table)
        
        # Depth chart
        self.order_book_depth = OrderBookDepth()
        right_layout.addWidget(self.order_book_depth)
        
        # Trading panel
        self.trading_panel = TradingPanel()
        self.trading_panel.set_socket_client(self.socket_client)
        right_layout.addWidget(self.trading_panel)
        
        # Portfolio
        portfolio_label = QLabel("Portfolio")
        portfolio_label.setStyleSheet("font-weight: bold;")
        right_layout.addWidget(portfolio_label)
        
        self.portfolio_table = PortfolioTable()
        self.portfolio_table.setMaximumHeight(200)
        right_layout.addWidget(self.portfolio_table)
        
        right_layout.addStretch()
        
        # Add panels to main layout
        main_layout.addWidget(left_panel, stretch=3)
        main_layout.addWidget(right_panel, stretch=1)
        
        # Status bar
        self.status_bar = QStatusBar()
        self.status_bar.setStyleSheet("color: #6c7086;")
        self.setStatusBar(self.status_bar)
        self.status_bar.showMessage("Disconnected")
    
    def _connect_signals(self):
        """Connect socket client signals to UI updates."""
        self.socket_client.connected.connect(self._on_connected)
        self.socket_client.disconnected.connect(self._on_disconnected)
        self.socket_client.market_data_received.connect(self._on_market_data)
        self.socket_client.execution_received.connect(self._on_execution)
        self.socket_client.error_received.connect(self._on_error)
    
    def _connect_to_server(self):
        """Connect to the exchange server."""
        self.event_log.log("Connecting to exchange server...", "INFO")
        if self.socket_client.connect_to_server():
            self.event_log.log("Connection attempt initiated", "INFO")
        else:
            self.event_log.log("Failed to connect", "ERROR")
    
    def _on_connected(self):
        """Handle successful connection."""
        self.status_bar.showMessage("Connected to Exchange Server")
        self.event_log.log("Connected to exchange server", "SUCCESS")
    
    def _on_disconnected(self):
        """Handle disconnection."""
        self.status_bar.showMessage("Disconnected")
        self.event_log.log("Disconnected from server", "WARNING")
    
    def _on_market_data(self, data: dict):
        """Handle market data update."""
        ticker = data.get('ticker', '')
        self.market_data_cache[ticker] = data
        
        # Update order book
        if ticker == self.current_ticker:
            bid = data.get('bid', 0)
            ask = data.get('ask', 0)
            last = data.get('last', 0)
            
            self.order_book_table.update_book(bid, ask)
            self.order_book_depth.update_order_book(
                bid, ask, 
                data.get('bid_size', 0), 
                data.get('ask_size', 0)
            )
            self.candlestick_chart.update_current_price(last)
            
            # Update price display
            self.price_label.setText(f"${last:.2f}")
        
        # Process for candlestick
        candle_result = self.data_parser.process_market_data(data)
        if candle_result and candle_result['ticker'] == self.current_ticker:
            self.candlestick_chart.add_candle(candle_result['candle'])
            self.volume_chart.update_volumes(
                self.data_parser.get_candles(self.current_ticker)
            )
    
    def _on_execution(self, data: dict):
        """Handle execution report."""
        order_id = data.get('order_id', 0)
        status = data.get('status', '')
        price = data.get('price', 0)
        quantity = data.get('quantity', 0)
        
        self.event_log.log(
            f"Order #{order_id}: {status} - {quantity} @ ${price:.2f}",
            "SUCCESS" if status == "FILLED" else "INFO"
        )
    
    def _on_error(self, message: str):
        """Handle error message."""
        self.event_log.log(message, "ERROR")
    
    def _on_ticker_changed(self, ticker: str):
        """Handle ticker selection change."""
        self.current_ticker = ticker
        self.candlestick_chart.set_ticker(ticker)
        self.trading_panel.ticker_combo.setCurrentText(ticker)
        
        # Update with cached data if available
        if ticker in self.market_data_cache:
            self._on_market_data(self.market_data_cache[ticker])
    
    def closeEvent(self, event):
        """Handle window close."""
        self.socket_client.disconnect()
        super().closeEvent(event)
