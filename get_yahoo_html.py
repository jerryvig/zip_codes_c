from xml.dom import minidom

import sys
import requests

def getTbodyNode(dom):
    for node in dom.documentElement.childNodes:
        if node.tagName == 'tbody':
    	    return node

def getAdjClosePrices(tbody):
    adjClosePrices = []
    for node in tbody.childNodes:
        if node.tagName == 'tr':
            tdCount = 0
            for child in node.childNodes:
                if child.tagName == 'td':
                    tdCount += 1
                    if tdCount == 6:
                        for span in child.childNodes:
                            adjClosePrices.append(span.childNodes[0].toxml().strip())
    return adjClosePrices

def main():
    for arg in sys.argv:
        print('arg = {}'.format(arg))

    response = requests.get('https://finance.yahoo.com/quote/SHOP/history?p=SHOP')
    table_start_idx = response.text.find(
        "<table class=\"W(100%) M(0)\" data-test=\"historical-prices\"")
    table_start = response.text[table_start_idx:]
    table_end_idx = table_start.find("<div class=")

    dom = minidom.parseString(table_start[:table_end_idx])
    tbody = getTbodyNode(dom)


    adjPrices = getAdjClosePrices(tbody)
    print(adjPrices)

if __name__ == '__main__':
    main()
