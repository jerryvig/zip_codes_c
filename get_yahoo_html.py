import requests
import sys

from html.parser import HTMLParser

class YahooParser(HTMLParser):
	def handle_starttag(self, tag, attrs):
		if tag == 'table':
			for attr in attrs:
				if attr[0] == 'data-test':
					pass

def main():
	for arg in sys.argv:
		print('arg = {}'.format(arg))

	response = requests.get('https://finance.yahoo.com/quote/SHOP/history?p=SHOP')
	# print(response.text)

	parser = YahooParser()
	parser.feed(response.text)
	
if __name__ == '__main__':
	main()
