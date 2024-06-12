
#pragma once

#include <iostream>
#include <vector>
#include <boost/container/flat_map.hpp>

struct BestPrice
{
    int64_t bidqty;
    int64_t bid;
    int64_t ask;
    int64_t askqty;

    BestPrice(int64_t bidqty, int64_t bid, int64_t askqty, int64_t ask)
        : bidqty(bidqty), bid(bid), askqty(askqty), ask(ask) {}

    BestPrice() : bidqty(0), bid(0), askqty(0), ask(0) {}

    friend std::ostream &operator<<(std::ostream &out, const BestPrice &best)
    {
        out << "BestPrice(" << best.bidqty << ", " << best.bid << ", "
            << best.ask << ", " << best.askqty << ")";
        return out;
    }
};

class OrderBook
{
public:
    struct Level
    {
        int64_t price;
        int64_t qty = 0;
        uint64_t seqno = 0;

        friend std::ostream &operator<<(std::ostream &out, const Level &level)
        {
            out << "Level(" << level.price << ", " << level.qty << ", " << level.seqno
                << ")";
            return out;
        }
    };

    OrderBook(void *data = NULL) : data_(NULL) {}

    BestPrice GetBestPrice() const
    {
        auto buy = buy_.rbegin(); // 买单此处也用反向迭代器是因为插入的时候price取了负
        auto sell = sell_.rbegin();
        BestPrice bp;
        if (buy != buy_.rend())
        {
            bp.bidqty = buy->second.qty;
            bp.bid = buy->second.price;
        }
        if (sell != sell_.rend())
        {
            bp.askqty = sell->second.qty;
            bp.ask = sell->second.price;
        }
        return bp;
    }

    void *GetUserData() const { return data_; }

    void SetUserData(void *data) { data_ = data; }

    bool Add(uint64_t seqno, bool buy_sell, int64_t price, int64_t qty)
    {
        if (qty <= 0)
        {
            return false;
        }
        auto &side = buy_sell ? buy_ : sell_;
        int64_t prio = buy_sell ? -price : price;
        auto it = side.insert(std::make_pair(prio, Level())).first;
        it->second.price = price;
        it->second.qty += qty;
        it->second.seqno = seqno;
        return it == side.begin();
    }

    bool Reduce(uint64_t seqno, bool buy_sell, int64_t price, int64_t qty)
    {
        auto &side = buy_sell ? buy_ : sell_;
        int64_t prio = buy_sell ? -price : price;
        auto it = side.find(prio);
        if (it == side.end())
        {
            return false;
        }
        it->second.qty -= qty;
        it->second.seqno = seqno;
        if (it->second.qty <= 0)
        {
            side.erase(it);
        }
        return it == side.begin();
    }

    bool IsCrossed()
    {
        if (buy_.empty() || sell_.empty())
        {
            return false;
        }
        return -buy_.rbegin()->first >= sell_.rbegin()->first;
    }

    void UnCross()
    {
        auto bit = buy_.begin();
        auto sit = sell_.begin();
        while (bit != buy_.end() && sit != sell_.end() && bit->second.price >= sit->second.price)
        {
            if (bit->second.seqno > sit->second.seqno)
            {
                sell_.erase(sit++);
            }
            else
            {
                buy_.erase(bit++);
            }
        }
    }

    friend std::ostream &operator<<(std::ostream &out, const OrderBook &book)
    {
        out << "Buy: " << std::endl;
        for (auto it = book.buy_.rbegin(); it != book.buy_.rend(); it++)
        {
            out << it->second << std::endl;
        }
        out << "Sell: " << std::endl;
        for (auto it = book.sell_.rbegin(); it != book.sell_.rend(); it++)
        {
            out << it->second << std::endl;
        }
        return out;
    }

private:
    void *data_ = nullptr;
    boost::container::flat_map<int64_t, Level, std::greater<int64_t>> buy_, sell_;
};

template <typename Handler>
class Feed
{
    static constexpr int16_t NOBOOK = std::numeric_limits<int16_t>::max();
    static constexpr int16_t MAXBOOK = std::numeric_limits<int16_t>::max();

    struct Order
    {
        int64_t price = 0;
        int32_t qty = 0;
        bool buy_sell = 0;
        int16_t bookid = NOBOOK;

        Order(int64_t price, int32_t qty, bool buy_sell, int16_t bookid)
            : price(price), qty(qty), buy_sell(buy_sell), bookid(bookid) {}
        Order() {}
    };

    static_assert(sizeof(Order) == 16, "");

public:
    Feed(Handler &handler, size_t size_hint, bool all_orders = false,
         bool all_books = false)
        : handler_(handler), all_orders_(all_orders), all_books_(all_books),
          // symbols_(16384,0),
          symbols_(16384),
          // orders_(size_hint, std::numeric_limits<uint64_t>::max()){
          orders_(size_hint)
    {
        size_hint_ = orders_.bucket_count();
    }

private:
    Feed(const Feed &) = delete;
    Feed &operator=(const Feed &) = delete;

    struct Hash
    {
        size_t operator()(uint64_t h) const noexcept
        {
            h ^= h >> 33;
            h *= 0xff51afd7ed558ccd;
            h ^= h >> 33;
            h *= 0xc4ceb9fe1a85ec53;
            h ^= h >> 33;
            reeturn h;
        }
    }

    Handler &handler;
    size_t size_hint_ = 0;
    bool all_orders_ = false;
    bool all_books_ = false;

    std::vector<OrderBook> books_;
    std::unordered_map<uint64_t, uint16_t, Hash> symbols_;
    std::unordered_map<uint64_t, Order, Hash> orders_;
};
