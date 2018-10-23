from xml.dom import minidom

import sys
import requests

def getTableDom(response):
    table_start_idx = response.text.find(
        "<table class=\"W(100%) M(0)\" data-test=\"historical-prices\"")
    table_start = response.text[table_start_idx:]
    table_end_idx = table_start.find("<div class=")
    return minidom.parseString(table_start[:table_end_idx])

def getTbodyNode(dom):
    for node in dom.documentElement.childNodes:
        if node.tagName == 'tbody':
            return node

def getAdjClosePrices(tbody):
    adj_close_prices = []
    for node in tbody.childNodes:
        if node.tagName == 'tr':
            td_count = 0
            for child in node.childNodes:
                if child.tagName == 'td':
                    td_count += 1
                    if td_count == 6:
                        span = child.childNodes[0]
                        adj_close_prices.append(float(span.childNodes[0].toxml().strip()))
    return adj_close_prices

def main():
    if len(sys.argv) < 2:
        print('No ticker argument supplied. Exiting....')
        return

    ticker = sys.argv[1].strip()
    url = 'https://finance.yahoo.com/quote/%s/history?p=%s' % (ticker, ticker)
    response = requests.get(url)
    dom = getTableDom(response)
    tbody = getTbodyNode(dom)

    adj_prices = getAdjClosePrices(tbody)
    print(adj_prices)

if __name__ == '__main__':
    main()
