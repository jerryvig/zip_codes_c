from xml.dom import minidom

import datetime
from datetime import date
import json
import sys
import time
import numpy
import requests

from termcolor import colored

def get_table_dom(response):
    table_start_idx = response.text.find(
        '<table class="W(100%) M(0)" data-test="historical-prices"')
    table_start = response.text[table_start_idx:]
    table_end_idx = table_start.find('</table>') + 8
    return minidom.parseString(table_start[:table_end_idx])

def get_crumb(response):
    crumbstore_start_idx = response.text.find("CrumbStore")
    json_start = response.text[crumbstore_start_idx + 12:crumbstore_start_idx + 70]
    json_end_idx = json_start.find("},")
    json_snippet = json_start[:json_end_idx + 1]
    json_obj = json.loads(json_snippet)
    return json_obj['crumb']

def get_timestamps():
    hoy = date.today()
    manana = hoy + datetime.timedelta(days=1)
    ago_366_days = hoy + datetime.timedelta(days=-366)
    manana_stamp = time.mktime(manana.timetuple())
    ago_366_days_stamp = time.mktime(ago_366_days.timetuple())
    return (manana_stamp, ago_366_days_stamp)

def get_title(response):
    title_start_idx = response.text.find('<title>')
    title_start = response.text[title_start_idx:]
    pipe_start = title_start.find('|') + 2
    hyphen_end = title_start.find('-')
    title = title_start[pipe_start:hyphen_end]
    return title

def get_tbody_node(dom):
    for node in dom.documentElement.childNodes:
        if node.tagName == 'tbody':
            return node
    return None

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
        for i in range(1, len(adj_prices_by_ticker[ticker])):
            changes.append(
                (adj_prices_by_ticker[ticker][i] - adj_prices_by_ticker[ticker][i-1])/
                adj_prices_by_ticker[ticker][i-1])
        changes_by_ticker[ticker] = changes
    return changes_by_ticker

def get_sigma_data_by_ticker(changes_by_ticker, titles_by_ticker):
    sigma_data_by_ticker = {}
    for ticker in changes_by_ticker:
        changes_numpy = numpy.array(changes_by_ticker[ticker][:-1])
        stdev = numpy.std(changes_numpy, ddof=1)
        sigma_change = changes_by_ticker[ticker][-1]/stdev

        print('changes length = %d' % len(changes_by_ticker[ticker]))

        sigma_data_by_ticker[ticker] = {
            'change': str(round(changes_by_ticker[ticker][-1] * 100, 3)) + '%',
            'title': titles_by_ticker[ticker],
            'sigma': str(round(stdev * 100, 3)) + '%',
            'sigma_change': round(sigma_change, 3),
        }
    return sigma_data_by_ticker

DOWN_DAYS = '3 consecutive down days: %s'
TWO_DOWN_DAYS = '2 consecutive down days: %s'
MONOTONIC_DECREASE = 'Monotonically decreasing: %s'
LAST_MOST_NEGATIVE = 'Last most negative: %s'
C_YES = colored('YES', 'green')
C_NO = colored('NO', 'red')

def print_monotonic_down(changes):
    if changes[2] < 0.0 and changes[2] < changes[1] and changes[2] < changes[0]:
        print(LAST_MOST_NEGATIVE % C_YES)
    else:
        print(LAST_MOST_NEGATIVE % C_NO)

    if changes[1] < 0.0 and changes[2] < 0.0:
        print(TWO_DOWN_DAYS % C_YES)
    else:
        print(TWO_DOWN_DAYS % C_NO)

    if changes[0] < 0.0 and changes[1] < 0.0 and changes[2] < 0.0:
        print(DOWN_DAYS % C_YES)
        if changes[1] < changes[0] and changes[2] < changes[1]:
            print(MONOTONIC_DECREASE % C_YES)
        else:
            print(MONOTONIC_DECREASE % C_NO)
    else:
        print(DOWN_DAYS % C_NO)
        print(MONOTONIC_DECREASE % C_NO)

def print_fit_strings(changes_by_ticker, adj_prices_by_ticker, titles_by_ticker):
    for ticker, changes in changes_by_ticker.items():
        exp_fit = 'exponential fit { '
        fit = 'Fit[{'
        fit += str(round(changes[0], 4)) + ', '
        exp_fit += str(round(changes[0], 4)) + ', '
        fit += str(round(changes[1], 4)) + ', '
        exp_fit += str(round(changes[1], 4)) + ', '
        fit += str(round(changes[2], 4))
        exp_fit += str(round(changes[2], 4))
        fit += '}, {x^2}, x]'
        exp_fit += ' }'
        print('%s: %.2f' % (ticker.upper(), adj_prices_by_ticker[ticker][3]))
        print(titles_by_ticker[ticker])
        print('Pricing data: %s' % str(adj_prices_by_ticker[ticker]))
        print_monotonic_down(changes)
        print(fit)
        print(exp_fit)

def main():
    if len(sys.argv) < 2:
        print('No ticker argument supplied. Exiting....')
        return

    adj_prices_by_ticker = {}
    titles_by_ticker = {}

    (manana_stamp, ago_366_days_stamp) = get_timestamps()

    for tick in sys.argv[1:]:
        ticker = tick.strip().upper()
        url = 'https://finance.yahoo.com/quote/%s/history?p=%s' % (ticker, ticker)
        print('url = %s' % url)

        response = requests.get(url)
        cookie_jar = response.cookies
        crumb = get_crumb(response)
        # print('CRUMB = %s' % crumb)

        download_url = ('https://query1.finance.yahoo.com/v7/finance/download/%s?'
                        'period1=%d&period2=%d&interval=1d&events=history'
                        '&crumb=%s' % (ticker, ago_366_days_stamp, manana_stamp, crumb))
        print('download_url = %s' % download_url)
        download_response = requests.get(download_url, cookies=cookie_jar)
        # print('DOWNLOAD RESPONSE TEXT = %s' % download_response.text)

        title = get_title(response)
        titles_by_ticker[ticker] = title

        lines = download_response.text.split('\n')
        print(lines[1:-1])
        sys.exit(0)

        dom = get_table_dom(response)
        tbody = get_tbody_node(dom)
        adj_prices = get_adj_close(tbody)
        adj_prices_by_ticker[ticker] = list(reversed(adj_prices))
        time.sleep(1.5)

    changes_by_ticker = get_changes_by_ticker(adj_prices_by_ticker)

    stdev_by_ticker = get_sigma_data_by_ticker(changes_by_ticker, titles_by_ticker)

    # print_fit_strings(changes_by_ticker, adj_prices_by_ticker, titles_by_ticker)
    print(json.dumps(stdev_by_ticker, sort_keys=True, indent=2))

if __name__ == '__main__':
    main()
