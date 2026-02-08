"""
Real-time candlestick chart using PyQtGraph.
"""

import numpy as np
import pyqtgraph as pg
from PyQt6.QtWidgets import QWidget, QVBoxLayout
from PyQt6.QtCore import Qt
from typing import List, Dict, Any


class CandlestickItem(pg.GraphicsObject):
    """Custom graphics item for drawing candlesticks."""
    
    def __init__(self):
        super().__init__()
        self.data = []
        self.picture = None
        
    def set_data(self, data: List[Dict[str, Any]]):
        """Update candlestick data."""
        self.data = data
        self.picture = None
        self.prepareGeometryChange()
        self.update()
    
    def generatePicture(self):
        """Generate the candlestick picture."""
        self.picture = pg.QtGui.QPicture()
        painter = pg.QtGui.QPainter(self.picture)
        
        if not self.data:
            painter.end()
            return
        
        w = 0.6  # Candle width
        
        for i, candle in enumerate(self.data):
            open_p = candle.get('open', 0)
            high = candle.get('high', 0)
            low = candle.get('low', 0)
            close = candle.get('close', 0)
            
            # Determine color
            if close >= open_p:
                color = pg.mkColor('#26a69a')  # Green
            else:
                color = pg.mkColor('#ef5350')  # Red
            
            painter.setPen(pg.mkPen(color, width=1))
            painter.setBrush(pg.mkBrush(color))
            
            # Draw wick (high-low line)
            painter.drawLine(pg.QtCore.QPointF(i, low), pg.QtCore.QPointF(i, high))
            
            # Draw body
            body_top = max(open_p, close)
            body_bottom = min(open_p, close)
            body_height = body_top - body_bottom
            
            if body_height < 0.001:
                body_height = 0.001
            
            rect = pg.QtCore.QRectF(i - w/2, body_bottom, w, body_height)
            painter.drawRect(rect)
        
        painter.end()
    
    def paint(self, painter, *args):
        if self.picture is None:
            self.generatePicture()
        if self.picture:
            self.picture.play(painter)
    
    def boundingRect(self):
        if self.picture is None:
            self.generatePicture()
        if self.picture:
            return pg.QtCore.QRectF(self.picture.boundingRect())
        return pg.QtCore.QRectF(0, 0, 1, 1)


class CandlestickChart(QWidget):
    """Real-time candlestick chart widget."""
    
    def __init__(self, parent=None):
        super().__init__(parent)
        self.ticker = ""
        self.candles = []
        
        self._setup_ui()
    
    def _setup_ui(self):
        layout = QVBoxLayout(self)
        layout.setContentsMargins(0, 0, 0, 0)
        
        # Create plot widget
        self.plot_widget = pg.PlotWidget()
        self.plot_widget.setBackground('#1e1e2e')
        self.plot_widget.showGrid(x=True, y=True, alpha=0.3)
        
        # Configure axes
        self.plot_widget.setLabel('left', 'Price', color='#cdd6f4')
        self.plot_widget.setLabel('bottom', 'Time', color='#cdd6f4')
        
        # Add candlestick item
        self.candle_item = CandlestickItem()
        self.plot_widget.addItem(self.candle_item)
        
        # Add current price line
        self.price_line = pg.InfiniteLine(
            angle=0, 
            movable=False, 
            pen=pg.mkPen('#fab387', width=1, style=Qt.PenStyle.DashLine)
        )
        self.plot_widget.addItem(self.price_line)
        
        layout.addWidget(self.plot_widget)
    
    def set_ticker(self, ticker: str):
        """Set the current ticker."""
        self.ticker = ticker
        self.candles = []
        self.candle_item.set_data([])
    
    def add_candle(self, candle: Dict[str, Any]):
        """Add a new completed candle."""
        self.candles.append(candle)
        
        # Keep last 200 candles
        if len(self.candles) > 200:
            self.candles = self.candles[-200:]
        
        self._update_chart()
    
    def update_current_price(self, price: float):
        """Update the current price line."""
        self.price_line.setValue(price)
    
    def _update_chart(self):
        """Redraw the chart with current data."""
        self.candle_item.set_data(self.candles)
        
        if self.candles:
            # Auto-range
            self.plot_widget.setXRange(0, len(self.candles), padding=0.05)
            
            prices = []
            for c in self.candles[-50:]:  # Recent 50 candles for range
                prices.extend([c['high'], c['low']])
            
            if prices:
                min_p = min(prices)
                max_p = max(prices)
                padding = (max_p - min_p) * 0.1
                self.plot_widget.setYRange(min_p - padding, max_p + padding)


class VolumeChart(QWidget):
    """Volume bar chart widget."""
    
    def __init__(self, parent=None):
        super().__init__(parent)
        self.volumes = []
        self.colors = []
        
        self._setup_ui()
    
    def _setup_ui(self):
        layout = QVBoxLayout(self)
        layout.setContentsMargins(0, 0, 0, 0)
        
        self.plot_widget = pg.PlotWidget()
        self.plot_widget.setBackground('#1e1e2e')
        self.plot_widget.setMaximumHeight(100)
        self.plot_widget.setLabel('left', 'Vol', color='#cdd6f4')
        
        # Volume bars
        self.bar_item = pg.BarGraphItem(x=[], height=[], width=0.6, brush='#6c7086')
        self.plot_widget.addItem(self.bar_item)
        
        layout.addWidget(self.plot_widget)
    
    def update_volumes(self, candles: List[Dict[str, Any]]):
        """Update volume bars."""
        if not candles:
            return
        
        x = list(range(len(candles)))
        heights = [c.get('volume', 0) for c in candles]
        
        # Color based on price direction
        brushes = []
        for c in candles:
            if c.get('close', 0) >= c.get('open', 0):
                brushes.append(pg.mkBrush('#26a69a80'))
            else:
                brushes.append(pg.mkBrush('#ef535080'))
        
        self.bar_item.setOpts(x=x, height=heights, brushes=brushes)
