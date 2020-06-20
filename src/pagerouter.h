/*
 *  SPDX-FileCopyrightText: 2020 Carson Black <uhhadd@gmail.com>
 *
 *  SPDX-License-Identifier: LGPL-2.0-or-later
 */

#pragma once

#include <QCache>
#include <QQuickItem>
#include <QRandomGenerator>
#include "columnview.h"

static std::map<quint32,QVariant> s_knownVariants;

struct ParsedRoute {
    QString name;
    QVariant data;
    bool cache;
    QPointer<QQuickItem> item;
    ParsedRoute(const ParsedRoute&) = delete;
    ~ParsedRoute() {
        if (item) {
            item->deleteLater();
        }
    }
    quint32 hash() {
        for (auto i = s_knownVariants.begin(); i != s_knownVariants.end(); i++) {
            if (i->second == data) {
                return i->first;
            }
        }
        auto number = QRandomGenerator::system()->generate();
        while (s_knownVariants.count(number) > 0) {
            number = QRandomGenerator::system()->generate();
        }
        s_knownVariants[number] = data;
        return number;
    }
    bool operator==(const ParsedRoute& rhs)
    {
        return name == rhs.name && data == rhs.data && item == rhs.item && cache == rhs.cache;
    }
    bool operator!=(const ParsedRoute& rhs)
    {
        return name != rhs.name && data != rhs.data && item != rhs.item && cache != rhs.cache;
    }
};

/**
 * Item representing a route the PageRouter can navigate to.
 * 
 * @include PageRoute.qml
 * 
 * @see PageRouter
 */
class PageRoute : public QObject
{
    Q_OBJECT

    /**
     * @brief The name of this route.
     * 
     * This name should be unique per PageRoute in a PageRouter.
     * When two PageRoutes have the same name, the one listed first
     * in the PageRouter will be used.
     */
    Q_PROPERTY(QString name MEMBER m_name READ name)

    /**
     * @brief The page component of this route.
     * 
     * This should be an instance of Component with a Kirigami::Page inside
     * of it.
     */
    Q_PROPERTY(QQmlComponent* component MEMBER m_component READ component)

    /**
     * @brief Whether pages generated by this route should be cached or not.
     * 
     * This should be an instance of Component with a Kirigami::Page inside
     * of it.
     * 
     * This will not work:
     * 
     * @include PageRouterCachePagesDont.qml
     * 
     * This will work:
     * 
     * @include PageRouterCachePagesDo.qml
     * 
     */
    Q_PROPERTY(bool cache MEMBER m_cache READ cache)

    /**
     * @brief How expensive this route is on resources.
     *
     * This will influence how long and how many pages from this
     * route will be kept in the PageRouter's cache.
     *
     */
    Q_PROPERTY(int cost MEMBER m_cost READ cost)

    Q_CLASSINFO("DefaultProperty", "component")

private:
    QString m_name;
    QQmlComponent* m_component;
    bool m_cache = false;
    int m_cost = 1;

public:
    QQmlComponent* component() { return m_component; };
    QString name() { return m_name; };
    bool cache() { return m_cache; };
    int cost() { return m_cost; };
};

class PageRouterAttached;

/**
 * An item managing pages and data of a ColumnView using named routes.
 * 
 * <br> <br>
 * 
 * ## Using a PageRouter
 * 
 * Applications typically manage their contents via elements called "pages" or "screens."
 * In Kirigami, these are called @link org::kde::kirigami::Page  Pages @endlink and are
 * arranged in @link PageRoute routes @endlink using a PageRouter to manage them. The PageRouter
 * manages a stack of @link org::kde::kirigami::Page Pages @endlink created from a pool of potential
 * @link PageRoute PageRoutes @endlink.
 * 
 * Unlike most traditional stacks, a PageRouter provides functions for random access to its pages
 * with navigateToRoute and routeActive.
 * 
 * When your user interface fits the stack paradigm and is likely to use random access navigation,
 * using the PageRouter is appropriate. For simpler navigation, it is more appropriate to avoid
 * the overhead of a PageRouter by using a @link org::kde::kirigami::PageRow PageRow  @endlink
 * instead.
 * 
 * <br> <br>
 * 
 * ## Navigation Model
 * 
 * A PageRouter draws from a pool of @link PageRoute PageRoutes @endlink in order to construct
 * its stack.
 * 
 * @image html PageRouterModel.svg width=50%
 * 
 * <br> <br>
 * 
 * You can push pages onto this stack...
 * 
 * @image html PageRouterPush.svg width=50%
 * 
 * ...or pop them off...
 * 
 * @image html PageRouterPop.svg width=50%
 * 
 * ...or navigate to an arbitrary collection of pages.
 * 
 * @image html PageRouterNavigate.svg width=50%
 * 
 * <br> <br>
 * 
 * Components are able to query the PageRouter about the currently active routes
 * on the stack. This is useful for e.g. a card indicating that the page it takes
 * the user to is currently active.
 * 
 * <br> <br>
 * 
 * ## Example
 * 
 * @include PageRouter.qml
 * 
 * @see PageRouterAttached
 * @see PageRoute
 */
class PageRouter : public QObject, public QQmlParserStatus
{
    Q_OBJECT
    Q_INTERFACES(QQmlParserStatus)

    /**
     * @brief The named routes a PageRouter can navigate to.
     * 
     * @include PageRouterRoutes.qml
     */
    Q_PROPERTY(QQmlListProperty<PageRoute> routes READ routes)

    Q_CLASSINFO("DefaultProperty", "routes")

    /**
     * @brief The initial route.
     * 
     * `initialRoute` is the page that the PageRouter will push upon
     * creation. Changing it after creation will cause the PageRouter to reset
     * its state. Not providing an `initialRoute` will result in undefined
     * behavior.
     * 
     * @include PageRouterInitialRoute.qml
     */
    Q_PROPERTY(QJSValue initialRoute READ initialRoute WRITE setInitialRoute NOTIFY initialRouteChanged)

    /**
     * @brief The ColumnView being puppeted by the PageRouter.
     * 
     * All PageRouters should be created with a ColumnView, and creating one without
     * a ColumnView is undefined behaviour.
     * 
     * @warning You should **not** directly interact with a ColumnView being puppeted
     * by a PageRouter. Instead, use a PageRouter's functions to manipulate the
     * ColumnView.
     * 
     * @include PageRouterColumnView.qml
     */
    Q_PROPERTY(ColumnView* pageStack MEMBER m_pageStack NOTIFY pageStackChanged)

private:
    /**
     * @brief The routes the PageRouter is aware of.
     * 
     * Generally, this should not be mutated from C++, only read.
     */
    QList<PageRoute*> m_routes;

    /**
     * @brief The PageRouter being puppeted.
     * 
     * m_pageRow is the ColumnView this PageRouter puppets.
     */
    ColumnView* m_pageStack = nullptr;

    /**
     * @brief The route that the PageRouter will load on completion.
     * 
     * m_initialRoute is the raw QJSValue from QML that will be
     * parsed into a ParsedRoute struct on construction.
     * Generally, this should not be mutated from C++, only read.
     */
    QJSValue m_initialRoute;

    /**
     * @brief The current routes pushed on the PageRow.
     * 
     * Generally, the state of m_pageRow and m_currentRoutes
     * should be kept in sync. Undesirable behaviour will result
     * from desynchronisation of the two.
     */
    QList<ParsedRoute*> m_currentRoutes;

    /**
     * @brief Cached routes.
     * 
     * A list of ParsedRoutes with instantiated items.
     */
    QMap<QPair<QString,quint32>,ParsedRoute*> m_cache;

    /**
     * @brief Helper function to push a route.
     * 
     * This function has the shared logic between
     * navigateToRoute and pushRoute.
     */
    void push(ParsedRoute* route);

    /**
     * @brief Helper function to access whether m_routes has a key.
     * 
     * This function abstracts the QJSValue.
     */
    bool routesContainsKey(const QString &key);

    /**
     * @brief Helper function to access the component of a key for m_routes.
     * 
     * The return value will be a nullptr if @p key does not exist in
     * m_routes.
     */
    QQmlComponent *routesValueForKey(const QString &key);

    /**
     * @brief Helper function to get the cost of a route.
     */
    int routesCostForKey(const QString &key);

    /**
     * @brief Helper function to access the cache status of a key for m_routes.
     * 
     * The return value will be false if @p key does not exist in
     * m_routes.
     */
    bool routesCacheForKey(const QString &key);

    void placeInCache(ParsedRoute *route);

    static void appendRoute(QQmlListProperty<PageRoute>* list, PageRoute*);
    static int routeCount(QQmlListProperty<PageRoute>* list);
    static PageRoute* route(QQmlListProperty<PageRoute>* list, int);
    static void clearRoutes(QQmlListProperty<PageRoute>* list);

    QVariant dataFor(QObject* object);
    bool isActive(QObject* object);
    void pushFromObject(QObject *object, QJSValue route);

    friend class PageRouterAttached;

protected:
    void classBegin() override;
    void componentComplete() override;

public:
    PageRouter(QQuickItem *parent = nullptr);
    ~PageRouter();

    QQmlListProperty<PageRoute> routes();

    QJSValue initialRoute() const;
    void setInitialRoute(QJSValue initialRoute);

    /**
     * @brief Navigate to the given route.
     * 
     * Calling `navigateToRoute` causes the PageRouter to replace currently
     * active pages with the new route.
     * 
     * @param route The given route for the PageRouter to navigate to.
     * A route is an array of variants or a single item. A string item will be interpreted
     * as a page without associated data. An object item will be interpreted
     * as follows:
     * @code{.js}
     * {
     *     "route": "/home" // The named page of the route.
     *     "data": QtObject {} // The data to pass to the page.
     * }
     * @endcode
     * Navigating to a route not defined in a PageRouter's routes is undefined
     * behavior.
     * 
     * @code{.qml}
     * Button {
     *     text: "Login"
     *     onClicked: {
     *         Kirigami.PageRouter.navigateToRoute(["/home", "/login"])
     *     }
     * }
     * @endcode
     */
    Q_INVOKABLE void navigateToRoute(QJSValue route);

    /**
     * @brief Check whether the current route is on the stack.
     * 
     * `routeActive` will return true if the given route
     * is on the stack.
     * 
     * @param route The given route to check for.
     * 
     * `routeActive` returns true for partial routes like
     * the following:
     * 
     * @code{.js}
     * Kirigami.PageRouter.navigateToRoute(["/home", "/login", "/google"])
     * Kirigami.PageRouter.routeActive(["/home", "/login"]) // returns true
     * @endcode
     * 
     * This only works from the root page, e.g. the following will return false:
     * @code{.js}
     * Kirigami.PageRouter.navigateToRoute(["/home", "/login", "/google"])
     * Kirigami.PageRouter.routeActive(["/login", "/google"]) // returns false
     * @endcode
     */
    Q_INVOKABLE bool routeActive(QJSValue route);

    /**
     * @brief Appends a route to the currently navigated route.
     * 
     * Calling `pushRoute` will append the given @p route to the currently navigated
     * routes. See navigateToRoute() if you want to replace the items currently on
     * the PageRouter.
     * 
     * @param route The given route to push.
     * 
     * @code{.js}
     * Kirigami.PageRouter.navigateToRoute(["/home", "/login"])
     * // The PageRouter is navigated to /home/login
     * Kirigami.PageRouter.pushRoute("/google")
     * // The PageRouter is navigated to /home/login/google
     * @endcode
     */
    Q_INVOKABLE void pushRoute(QJSValue route);

    /**
     * @brief Pops the last page on the router.
     * 
     * Calling `popRoute` will result in the last page on the router getting popped.
     * You should not call this function when there is only one page on the router.
     * 
     * @code{.js}
     * Kirigami.PageRouter.navigateToRoute(["/home", "/login"])
     * // The PageRouter is navigated to /home/login
     * Kirigami.PageRouter.popRoute()
     * // The PageRouter is navigated to /home
     * @endcode
     */
    Q_INVOKABLE void popRoute();

    /**
     * @brief Shifts keyboard focus and view to a given index on the PageRouter's stack.
     * 
     * @param view The view to bring to focus. If this is an integer index, the PageRouter will
     * navigate to the given index. If it's a route specifier, the PageRouter will navigate
     * to the first route matching it.
     * 
     * Navigating to route by index:
     * @code{.js}
     * Kirigami.PageRouter.navigateToRoute(["/home", "/browse", "/apps", "/login"])
     * Kirigami.PageRouter.bringToView(1)
     * @endcode
     * 
     * Navigating to route by name:
     * @code{.js}
     * Kirigami.PageRouter.navigateToRoute(["/home", "/browse", "/apps", "/login"])
     * Kirigami.PageRouter.bringToView("/browse")
     * @endcode
     * 
     * Navigating to route by data:
     * @code{.js}
     * Kirigami.PageRouter.navigateToRoute([{"route": "/page", "data": "red"},
     *                                      {"route": "/page", "data": "blue"},
     *                                      {"route": "/page", "data": "green"},
     *                                      {"route": "/page", "data": "yellow"}])
     * Kirigami.PageRouter.bringToView({"route": "/page", "data": "blue"})
     * @endcode
     */
    Q_INVOKABLE void bringToView(QJSValue route);

    /**
     * @brief Returns a QJSValue corresponding to the current pages on the stack.
     * 
     * The returned value is in the same form as the input to navigateToRoute.
     */
    Q_INVOKABLE QJSValue currentRoutes() const;

    static PageRouterAttached *qmlAttachedProperties(QObject *object);

Q_SIGNALS:
    void routesChanged();
    void initialRouteChanged();
    void pageStackChanged();
    void currentIndexChanged();
    void navigationChanged();
};

/**
 * Attached object allowing children of a PageRouter to access its functions
 * without requiring the children to have the parent PageRouter's id.
 * 
 * @see PageRouter
 */
class PageRouterAttached : public QObject
{
    Q_OBJECT

    Q_PROPERTY(PageRouter *router READ router WRITE setRouter NOTIFY routerChanged)
    /**
     * The data for the page this item belongs to. Accessing this property
     * outside of a PageRouter will result in undefined behavior.
     */
    Q_PROPERTY(QVariant data READ data MEMBER m_data NOTIFY dataChanged)

    /**
     * Whether the page this item belongs to is the current index of the ColumnView.
     * Accessing this property outside of a PageRouter will result in undefined behaviour.
     */
    Q_PROPERTY(bool isCurrent READ isCurrent NOTIFY isCurrentChanged)
    
    /**
     * Which route this PageRouterAttached should watch for.
     * 
     * @include PageRouterWatchedRoute.qml
     */
    Q_PROPERTY(QJSValue watchedRoute READ watchedRoute WRITE setWatchedRoute NOTIFY watchedRouteChanged)

    /**
     * Whether the watchedRoute is currently active.
     */
    Q_PROPERTY(bool watchedRouteActive READ watchedRouteActive NOTIFY navigationChanged)

private:
    explicit PageRouterAttached(QObject *parent = nullptr);

    QPointer<PageRouter> m_router;
    QVariant m_data;
    QJSValue m_watchedRoute;

    void findParent();

    friend class PageRouter;

public:
    PageRouter* router() const { return m_router; };
    void setRouter(PageRouter* router) {
    	m_router = router;
    	Q_EMIT routerChanged();
    }
    QVariant data() const;
    bool isCurrent() const;
    /// @see PageRouter::navigateToRoute()
    Q_INVOKABLE void navigateToRoute(QJSValue route);
    /// @see PageRouter::routeActive()
    Q_INVOKABLE bool routeActive(QJSValue route);
    /// @see PageRouter::pushRoute()
    Q_INVOKABLE void pushRoute(QJSValue route);
    /// @see PageRouter::popRoute()
    Q_INVOKABLE void popRoute();
    // @see PageRouter::bringToView()
    Q_INVOKABLE void bringToView(QJSValue route);
    /**
     * @brief Push a route from this route on the stack.
     * 
     * Replace the routes after the route this is invoked on
     * with the provided @p route.
     * 
     * For example, if you invoke this method on the second route
     * in the PageRouter's stack, routes after the second
     * route will be replaced with the provided routes.
     */
    Q_INVOKABLE void pushFromHere(QJSValue route);
    /**
     * @brief Pop routes after this route on the stack.
     * 
     * Pop the routes after the route this is invoked on with
     * the provided @p route.
     * 
     * For example, if you invoke this method on the second route
     * in the PageRouter's stack, routes after the second route
     * will be removed from the stack.
     */
    Q_INVOKABLE void popFromHere();
    bool watchedRouteActive();
    void setWatchedRoute(QJSValue route);
    QJSValue watchedRoute();

Q_SIGNALS:
    void routerChanged();
    void dataChanged();
    void isCurrentChanged();
    void navigationChanged();
    void watchedRouteChanged();
};

QML_DECLARE_TYPEINFO(PageRouter, QML_HAS_ATTACHED_PROPERTIES)
