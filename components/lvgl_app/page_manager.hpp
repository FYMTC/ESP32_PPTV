#pragma once
#include <stack>
#include "page.hpp"
class PageManager {
public:
    static PageManager& instance();
    void push(Page* page, bool destroyPrev = false);
    void pop();
    void replace(Page* page, bool destroyPrev = true);
    Page* current();
private:
    PageManager();
    std::stack<Page*> pageStack;
};
