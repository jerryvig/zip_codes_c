import requests
import sys

def main():
	for arg in sys.argv:
		print('arg = {}'.format(arg))

	response = requests.get('https://finance.yahoo.com/quote/SHOP/history?p=SHOP')
	table_start_idx = response.text.find("<table class=\"W(100%) M(0)\" data-test=\"historical-prices\"")
	table_start = response.text[table_start_idx:]
	table_end_idx = table_start.find("<div class=")
	print(table_start[:table_end_idx])
	
if __name__ == '__main__':
	main()
