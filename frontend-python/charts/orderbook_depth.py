"""
Order book depth visualization widget.
"""

import pyqtgraph as pg
from PyQt6.QtWidgets import QWidget, QVBoxLayout, QHBoxLayout, QLabel
from PyQt6.QtCore import Qt
from typing import List, Dict, Any, Tuple


class OrderBookDepth(QWidget):
    """Order book depth chart showing bid/ask volume levels."""
    
    def __init__(self, parent=None):
        super().__init__(parent)
        self.bid_levels: List[Tuple[float, int]] = []  # (price, size)
        self.ask_levels: List[Tuple[float, int]] = []
        
        self._setup_ui()
    
    def _setup_ui(self):
        layout = QVBoxLayout(self)
        layout.setContentsMargins(5, 5, 5, 5)
        
        # Title
        title = QLabel("Order Book Depth")
        title.setStyleSheet("color: #cdd6f4; font-weight: bold; font-size: 14px;")
        title.setAlignment(Qt.AlignmentFlag.AlignCenter)
        layout.addWidget(title)
        
        # Best bid/ask display
        price_layout = QHBoxLayout()
        
        self.best_bid_label = QLabel("Bid: --")
        self.best_bid_label.setStyleSheet("color: #26a69a; font-size: 16px; font-weight: bold;")
        
        self.spread_label = QLabel("Spread: --")
        self.spread_label.setStyleSheet("color: #6c7086; font-size: 12px;")
        self.spread_label.setAlignment(Qt.AlignmentFlag.AlignCenter)
        
        self.best_ask_label = QLabel("Ask: --")
        self.best_ask_label.setStyleSheet("color: #ef5350; font-size: 16px; font-weight: bold;")
        self.best_ask_label.setAlignment(Qt.AlignmentFlag.AlignRight)
        
        price_layout.addWidget(self.best_bid_label)
        price_layout.addWidget(self.spread_label)
        price_layout.addWidget(self.best_ask_label)
        layout.addLayout(price_layout)
        
        # Depth chart
        self.plot_widget = pg.PlotWidget()
        self.plot_widget.setBackground('#1e1e2e')
        self.plot_widget.setLabel('left', 'Cumulative Size', color='#cdd6f4')
        self.plot_widget.setLabel('bottom', 'Price', color='#cdd6f4')
        
        # Create plot items
        self.bid_curve = pg.PlotCurveItem(
            pen=pg.mkPen('#26a69a', width=2),
            fillLevel=0,
            brush=pg.mkBrush('#26a69a40')
        )
        self.ask_curve = pg.PlotCurveItem(
            pen=pg.mkPen('#ef5350', width=2),
            fillLevel=0,
            brush=pg.mkBrush('#ef535040')
        )
        
        self.plot_widget.addItem(self.bid_curve)
        self.plot_widget.addItem(self.ask_curve)
        
        layout.addWidget(self.plot_widget)
    
    def update_order_book(self, bid: float, ask: float, 
                          bid_size: int = 0, ask_size: int = 0):
        """Update the order book display with market data."""
        # Update labels
        if bid > 0:
            self.best_bid_label.setText(f"Bid: ${bid:.2f}")
        else:
            self.best_bid_label.setText("Bid: --")
        
        if ask > 0:
            self.best_ask_label.setText(f"Ask: ${ask:.2f}")
        else:
            self.best_ask_label.setText("Ask: --")
        
        if bid > 0 and ask > 0:
            spread = ask - bid
            spread_bps = (spread / ((bid + ask) / 2)) * 10000
            self.spread_label.setText(f"Spread: ${spread:.2f} ({spread_bps:.1f} bps)")
        else:
            self.spread_label.setText("Spread: --")
        
        # Generate synthetic depth data for visualization
        # In a real implementation, this would come from the order book
        self._generate_depth_data(bid, ask, bid_size, ask_size)
        self._update_chart()
    
    def _generate_depth_data(self, best_bid: float, best_ask: float,
                             bid_size: int, ask_size: int):
        """Generate simulated depth levels for visualization."""
        if best_bid <= 0 or best_ask <= 0:
            self.bid_levels = []
            self.ask_levels = []
            return
        
        # Generate 10 levels each side
        num_levels = 10
        level_spacing = (best_ask - best_bid) / 10 if best_ask > best_bid else 0.01
        
        self.bid_levels = []
        self.ask_levels = []
        
        cumulative_bid = 0
        cumulative_ask = 0
        
        for i in range(num_levels):
            # Bid levels (descending from best bid)
            bid_price = best_bid - i * level_spacing
            bid_qty = int(bid_size * (1 + i * 0.5) if bid_size > 0 else 100 * (1 + i * 0.5))
            cumulative_bid += bid_qty
            self.bid_levels.append((bid_price, cumulative_bid))
            
            # Ask levels (ascending from best ask)
            ask_price = best_ask + i * level_spacing
            ask_qty = int(ask_size * (1 + i * 0.5) if ask_size > 0 else 100 * (1 + i * 0.5))
            cumulative_ask += ask_qty
            self.ask_levels.append((ask_price, cumulative_ask))
    
    def _update_chart(self):
        """Update the depth chart."""
        if self.bid_levels:
            bid_prices = [level[0] for level in self.bid_levels]
            bid_sizes = [level[1] for level in self.bid_levels]
            self.bid_curve.setData(x=bid_prices, y=bid_sizes)
        else:
            self.bid_curve.setData(x=[], y=[])
        
        if self.ask_levels:
            ask_prices = [level[0] for level in self.ask_levels]
            ask_sizes = [level[1] for level in self.ask_levels]
            self.ask_curve.setData(x=ask_prices, y=ask_sizes)
        else:
            self.ask_curve.setData(x=[], y=[])


class OrderBookTable(QWidget):
    """Simple order book table widget."""
    
    def __init__(self, parent=None):
        super().__init__(parent)
        self._setup_ui()
    
    def _setup_ui(self):
        layout = QVBoxLayout(self)
        layout.setContentsMargins(5, 5, 5, 5)
        
        title = QLabel("Order Book")
        title.setStyleSheet("color: #cdd6f4; font-weight: bold; font-size: 14px;")
        title.setAlignment(Qt.AlignmentFlag.AlignCenter)
        layout.addWidget(title)
        
        # Headers
        header_layout = QHBoxLayout()
        for text in ["Size", "Bid", "Ask", "Size"]:
            lbl = QLabel(text)
            lbl.setStyleSheet("color: #6c7086; font-size: 11px;")
            lbl.setAlignment(Qt.AlignmentFlag.AlignCenter)
            header_layout.addWidget(lbl)
        layout.addLayout(header_layout)
        
        # Price rows (5 levels)
        self.bid_labels = []
        self.ask_labels = []
        self.bid_size_labels = []
        self.ask_size_labels = []
        
        for i in range(5):
            row_layout = QHBoxLayout()
            
            bid_size = QLabel("--")
            bid_size.setStyleSheet("color: #26a69a; font-size: 12px;")
            bid_size.setAlignment(Qt.AlignmentFlag.AlignRight)
            self.bid_size_labels.append(bid_size)
            
            bid_price = QLabel("--")
            bid_price.setStyleSheet("color: #26a69a; font-weight: bold; font-size: 12px;")
            bid_price.setAlignment(Qt.AlignmentFlag.AlignCenter)
            self.bid_labels.append(bid_price)
            
            ask_price = QLabel("--")
            ask_price.setStyleSheet("color: #ef5350; font-weight: bold; font-size: 12px;")
            ask_price.setAlignment(Qt.AlignmentFlag.AlignCenter)
            self.ask_labels.append(ask_price)
            
            ask_size = QLabel("--")
            ask_size.setStyleSheet("color: #ef5350; font-size: 12px;")
            ask_size.setAlignment(Qt.AlignmentFlag.AlignLeft)
            self.ask_size_labels.append(ask_size)
            
            row_layout.addWidget(bid_size)
            row_layout.addWidget(bid_price)
            row_layout.addWidget(ask_price)
            row_layout.addWidget(ask_size)
            layout.addLayout(row_layout)
    
    def update_book(self, bid: float, ask: float):
        """Update the order book display."""
        # Generate simulated levels
        spread = ask - bid if bid > 0 and ask > 0 else 0.01
        tick = spread / 10 if spread > 0 else 0.01
        
        for i in range(5):
            if bid > 0:
                level_bid = bid - i * tick
                self.bid_labels[i].setText(f"{level_bid:.2f}")
                self.bid_size_labels[i].setText(str(100 * (i + 1)))
            else:
                self.bid_labels[i].setText("--")
                self.bid_size_labels[i].setText("--")
            
            if ask > 0:
                level_ask = ask + i * tick
                self.ask_labels[i].setText(f"{level_ask:.2f}")
                self.ask_size_labels[i].setText(str(100 * (i + 1)))
            else:
                self.ask_labels[i].setText("--")
                self.ask_size_labels[i].setText("--")
