from xml.dom import minidom

import sys
import time
import requests

def get_table_dom(response):
    table_start_idx = response.text.find(
        "<table class=\"W(100%) M(0)\" data-test=\"historical-prices\"")
    table_start = response.text[table_start_idx:]
    table_end_idx = table_start.find("<div class=")
    return minidom.parseString(table_start[:table_end_idx])

def get_tbody_node(dom):
    for node in dom.documentElement.childNodes:
        if node.tagName == 'tbody':
            return node

def get_adj_close(tbody):
    adj_close_prices = []
    for node in tbody.childNodes:
        if node.tagName == 'tr':
            td_count = 0
            for child in node.childNodes:
                if child.tagName == 'td':
                    td_count += 1
                    if td_count == 6:
                        span = child.childNodes[0]
                        clean_adj_close = float(span.childNodes[0].toxml().strip().replace(',', ''))
                        adj_close_prices.append(clean_adj_close)
    return adj_close_prices

def main():
    if len(sys.argv) < 2:
        print('No ticker argument supplied. Exiting....')
        return

    for tick in sys.argv[1:]:
        ticker = tick.strip()
        url = 'https://finance.yahoo.com/quote/%s/history?p=%s' % (ticker, ticker)
        response = requests.get(url)
        dom = get_table_dom(response)
        tbody = get_tbody_node(dom)
        adj_prices = get_adj_close(tbody)
        print(adj_prices)
        time.sleep(1)

if __name__ == '__main__':
    main()
