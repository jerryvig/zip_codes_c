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
    manana = date.today() + datetime.timedelta(days=1)
    ago_366_days = manana + datetime.timedelta(days=-367)
    manana_stamp = time.mktime(manana.timetuple())
    ago_366_days_stamp = time.mktime(ago_366_days.timetuple())
    return (manana_stamp, ago_366_days_stamp)

def get_title(response):
    title_start_idx = response.text.find('<title>')
    title_start = response.text[title_start_idx:]
    pipe_start = title_start.find('|') + 2
    hyphen_end = title_start.find('-')
    return title_start[pipe_start:hyphen_end].strip()

def get_tbody_node(dom):
    for node in dom.documentElement.childNodes:
        if node.tagName == 'tbody':
            return node
    return None

def get_adj_close(response_text):
    lines = response_text.split('\n')
    data_lines = lines[1:-1]
    adj_prices = []
    for line in data_lines:
        cols = line.split(',')
        adj_prices.append(float(cols[5]))
    return adj_prices

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
        changes_0 = numpy.array(changes_by_ticker[ticker][1:-1])
        changes_minus_one = numpy.array(changes_by_ticker[ticker][:-2])

        changes_tuple = []
        for idx, ele in enumerate(changes_0):
            changes_tuple.append((changes_minus_one[idx], ele))

        srted = list(reversed(sorted(changes_tuple, key=lambda b: b[0])))[:10]

        pct_sum = 0
        for ele in srted:
            pct_sum += -0.5*numpy.sign(ele[0] * ele[1]) + 0.5

        self_correlation = numpy.corrcoef([changes_minus_one, changes_0])[1, 0]

        changes_numpy = numpy.array(changes_by_ticker[ticker][:-1])
        stdev = numpy.std(changes_numpy, ddof=1)
        sigma_change = changes_by_ticker[ticker][-1]/stdev

        sigma_data_by_ticker[ticker] = {
            'c_name': titles_by_ticker[ticker],
            'change': str(round(changes_by_ticker[ticker][-1] * 100, 3)) + '%',
            'record_count': len(changes_by_ticker[ticker]),
            'self_correlation': str(round(self_correlation * 100, 3)) + '%',
            'sigma': str(round(stdev * 100, 3)) + '%',
            'sigma_change': round(sigma_change, 3),
            'sign_diff_pct':  str(round(pct_sum * 10, 4)) + '%'
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
        print('No ticker argument given. Exiting....')
        return

    adj_prices_by_ticker = {}
    titles_by_ticker = {}

    (manana_stamp, ago_366_days_stamp) = get_timestamps()
    symbol_count = 0

    for symbol in sys.argv[1:]:
        ticker = symbol.strip().upper()
        url = 'https://finance.yahoo.com/quote/%s/history?p=%s' % (ticker, ticker)
        print('url = %s' % url)

        response = requests.get(url)
        cookie_jar = response.cookies
        crumb = get_crumb(response)

        titles_by_ticker[ticker] = get_title(response)

        download_url = ('https://query1.finance.yahoo.com/v7/finance/download/%s?'
                        'period1=%d&period2=%d&interval=1d&events=history'
                        '&crumb=%s' % (ticker, ago_366_days_stamp, manana_stamp, crumb))
        print('download_url = %s' % download_url)
        download_response = requests.get(download_url, cookies=cookie_jar)

        adj_prices_by_ticker[ticker] = get_adj_close(download_response.text)

        symbol_count += 1
        if symbol_count < len(sys.argv[1:]):
            time.sleep(1.5)

    changes_by_ticker = get_changes_by_ticker(adj_prices_by_ticker)

    stdev_by_ticker = get_sigma_data_by_ticker(changes_by_ticker, titles_by_ticker)

    # print_fit_strings(changes_by_ticker, adj_prices_by_ticker, titles_by_ticker)
    print(json.dumps(stdev_by_ticker, sort_keys=True, indent=2))

if __name__ == '__main__':
    main()
