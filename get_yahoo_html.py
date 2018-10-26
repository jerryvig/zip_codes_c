from xml.dom import minidom

import json
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

def get_changes_by_ticker(adj_prices_by_ticker):
    changes_by_ticker = {}
    for ticker in adj_prices_by_ticker:
        changes = []
        changes.append((adj_prices_by_ticker[ticker][1] - adj_prices_by_ticker[ticker][0])/
                       adj_prices_by_ticker[ticker][0])
        changes.append((adj_prices_by_ticker[ticker][2] - adj_prices_by_ticker[ticker][1])/
                       adj_prices_by_ticker[ticker][1])
        changes.append((adj_prices_by_ticker[ticker][3] - adj_prices_by_ticker[ticker][2])/
                       adj_prices_by_ticker[ticker][2])
        changes_by_ticker[ticker] = changes
    return changes_by_ticker

def print_fit_strings(changes_by_ticker, adj_prices_by_ticker):
    for ticker, changes in changes_by_ticker.items():
        exp_fit = 'exponential fit { '
        fit = 'Fit[{'
        fit += str(round(changes[0]*100, 4)) + ', '
        exp_fit += str(round(changes[0]*100, 4)) + ', '
        fit += str(round(changes[1]*100, 4)) + ', '
        exp_fit += str(round(changes[1]*100, 4)) + ', '
        fit += str(round(changes[2]*100, 4))
        exp_fit += str(round(changes[2]*100, 4))
        fit += '}, {x^2}, x]'
        exp_fit += ' }'
        print("%s: %.2f" % (ticker.upper(), adj_prices_by_ticker[ticker][3]))
        print("Pricing data: %s" % str(adj_prices_by_ticker[ticker]))
        print(fit)
        print(exp_fit)    

def main():
    if len(sys.argv) < 2:
        print('No ticker argument supplied. Exiting....')
        return

    adj_prices_by_ticker = {}

    for tick in sys.argv[1:]:
        ticker = tick.strip().upper()
        url = 'https://finance.yahoo.com/quote/%s/history?p=%s' % (ticker, ticker)
        response = requests.get(url)
        dom = get_table_dom(response)
        tbody = get_tbody_node(dom)
        adj_prices = get_adj_close(tbody)
        adj_prices_by_ticker[ticker] = list(reversed(adj_prices[:4]))
        time.sleep(1.5)

    changes_by_ticker = get_changes_by_ticker(adj_prices_by_ticker)

    print_fit_strings(changes_by_ticker, adj_prices_by_ticker)

    print(json.dumps(changes_by_ticker, sort_keys=True, indent=2))

if __name__ == '__main__':
    main()
