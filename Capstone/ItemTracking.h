#pragma once

#include <string>

class ItemTracking				
{
	public:
		ItemTracking();			
		void MainMenu();
		int AddItem();
		int FindItem();
		void ListItems();				
		void GraphItems();
		void Analytics();
		//double Costing(const std::string& itemName);
		double ScrapePrice(const std::string& itemName);
		
	private:
		int choice = 0;			
};
