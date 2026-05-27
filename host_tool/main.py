"""上位机入口"""

import argparse
import sys

from config import DEFAULT_BAUD
from gui_app import App


def main():
    parser = argparse.ArgumentParser(description="环境监测仪上位机工具")
    parser.add_argument("--port", "-p", default="", help="串口号, 如 COM3")
    parser.add_argument("--baud", "-b", type=int, default=DEFAULT_BAUD, help="波特率")
    args = parser.parse_args()

    app = App(port=args.port, baud=args.baud)
    app.mainloop()


if __name__ == "__main__":
    main()
