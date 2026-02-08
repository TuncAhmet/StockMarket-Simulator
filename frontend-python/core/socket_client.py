"""
Socket client for communicating with the C Exchange Engine.
Uses QThread for async networking to avoid blocking the UI.
"""

import json
import socket
import threading
from typing import Optional, Callable, Dict, Any
from PyQt6.QtCore import QThread, pyqtSignal, QObject


class SocketClient(QObject):
    """Async socket client for the exchange server."""
    
    # Signals for UI updates
    connected = pyqtSignal()
    disconnected = pyqtSignal()
    market_data_received = pyqtSignal(dict)
    execution_received = pyqtSignal(dict)
    error_received = pyqtSignal(str)
    
    def __init__(self, host: str = "127.0.0.1", port: int = 8080):
        super().__init__()
        self.host = host
        self.port = port
        self.socket: Optional[socket.socket] = None
        self.running = False
        self.receive_thread: Optional[threading.Thread] = None
        self.buffer = ""
        
    def connect_to_server(self) -> bool:
        """Connect to the exchange server."""
        try:
            self.socket = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
            self.socket.connect((self.host, self.port))
            self.socket.setblocking(False)
            self.running = True
            
            # Start receive thread
            self.receive_thread = threading.Thread(target=self._receive_loop, daemon=True)
            self.receive_thread.start()
            
            self.connected.emit()
            return True
        except Exception as e:
            self.error_received.emit(f"Connection failed: {e}")
            return False
    
    def disconnect(self):
        """Disconnect from the server."""
        self.running = False
        if self.socket:
            try:
                self.socket.close()
            except:
                pass
            self.socket = None
        self.disconnected.emit()
    
    def _receive_loop(self):
        """Background thread for receiving data."""
        while self.running and self.socket:
            try:
                data = self.socket.recv(4096)
                if not data:
                    self.running = False
                    self.disconnected.emit()
                    break
                
                self.buffer += data.decode('utf-8')
                self._process_buffer()
                
            except BlockingIOError:
                # No data available, sleep briefly
                import time
                time.sleep(0.01)
            except Exception as e:
                if self.running:
                    self.error_received.emit(f"Receive error: {e}")
                break
        
        self.running = False
    
    def _process_buffer(self):
        """Process complete JSON messages from the buffer."""
        while '\n' in self.buffer:
            line, self.buffer = self.buffer.split('\n', 1)
            if line.strip():
                try:
                    msg = json.loads(line)
                    self._handle_message(msg)
                except json.JSONDecodeError as e:
                    self.error_received.emit(f"JSON error: {e}")
    
    def _handle_message(self, msg: Dict[str, Any]):
        """Route message to appropriate signal."""
        msg_type = msg.get('type', '')
        
        if msg_type == 'MARKET_DATA':
            self.market_data_received.emit(msg)
        elif msg_type == 'EXECUTION_REPORT':
            self.execution_received.emit(msg)
        elif msg_type == 'ERROR':
            self.error_received.emit(msg.get('message', 'Unknown error'))
    
    def send_order(self, ticker: str, side: str, order_type: str, 
                   price: float, quantity: int) -> bool:
        """Send a new order to the server."""
        msg = {
            'type': 'ORDER_NEW',
            'ticker': ticker,
            'side': side,
            'type': order_type,
            'price': price,
            'quantity': quantity
        }
        return self._send_message(msg)
    
    def cancel_order(self, ticker: str, order_id: int) -> bool:
        """Send a cancel request to the server."""
        msg = {
            'type': 'ORDER_CANCEL',
            'ticker': ticker,
            'order_id': order_id
        }
        return self._send_message(msg)
    
    def _send_message(self, msg: Dict[str, Any]) -> bool:
        """Send a JSON message to the server."""
        if not self.socket or not self.running:
            return False
        
        try:
            data = json.dumps(msg) + '\n'
            self.socket.sendall(data.encode('utf-8'))
            return True
        except Exception as e:
            self.error_received.emit(f"Send error: {e}")
            return False


class DataParser:
    """Parse and aggregate market data for charting."""
    
    def __init__(self):
        self.candles: Dict[str, list] = {}  # ticker -> list of OHLCV candles
        self.current_candle: Dict[str, dict] = {}  # ticker -> current forming candle
        self.candle_interval = 5.0  # seconds
        
    def process_market_data(self, data: Dict[str, Any]) -> Optional[Dict[str, Any]]:
        """Process market data update and return completed candle if any."""
        ticker = data.get('ticker', '')
        if not ticker:
            return None
        
        last_price = data.get('last', 0)
        timestamp = data.get('timestamp', 0) / 1_000_000  # Convert from microseconds
        
        if ticker not in self.candles:
            self.candles[ticker] = []
            self.current_candle[ticker] = None
        
        current = self.current_candle[ticker]
        
        if current is None or (timestamp - current['start_time']) >= self.candle_interval:
            # Complete current candle and start new one
            completed = current
            
            self.current_candle[ticker] = {
                'start_time': timestamp,
                'open': last_price,
                'high': last_price,
                'low': last_price,
                'close': last_price,
                'volume': data.get('last_size', 0)
            }
            
            if completed:
                self.candles[ticker].append(completed)
                return {'ticker': ticker, 'candle': completed}
        else:
            # Update current candle
            current['high'] = max(current['high'], last_price)
            current['low'] = min(current['low'], last_price)
            current['close'] = last_price
            current['volume'] += data.get('last_size', 0)
        
        return None
    
    def get_candles(self, ticker: str, count: int = 100) -> list:
        """Get recent candles for a ticker."""
        if ticker not in self.candles:
            return []
        return self.candles[ticker][-count:]
